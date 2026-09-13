import { spawnSync } from 'node:child_process';
import { readFile,writeFile,mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
export function parseCorpus(text,jsonl=false) {
  return text.replace(/^\uFEFF/,'').split(/\r?\n/).filter(x=>x.trim() && !x.startsWith('#')).map((line,index)=>{
    const row=jsonl?JSON.parse(line):{fen:line};
    if(typeof row.fen!=='string' || row.fen.trim().split(/\s+/).length!==6) throw Error(`Invalid FEN record ${index+1}`);
    if(row.cp_loss!=null && (!Number.isFinite(row.cp_loss) || row.cp_loss<0)) throw Error('Invalid cp_loss');
    if(row.reference_move!=null && !/^[a-h][1-8][a-h][1-8][qrbn]?$/.test(row.reference_move)) throw Error('Invalid reference_move');
    return row;
  });
}
export function quality(samples) {
  const metric=(data,predict)=>{
    if(!data.length) return {n:0,mae:null,median:null,p90:null,sign_agreement:null};
    const errors=data.map(s=>Math.abs(predict(s)-s.score)).sort((a,b)=>a-b);
    return {n:data.length,mae:errors.reduce((a,b)=>a+b,0)/errors.length,median:(errors[Math.floor((errors.length-1)/2)]+errors[Math.floor(errors.length/2)])/2,p90:errors[Math.ceil(errors.length*.9)-1],sign_agreement:data.filter(s=>Math.sign(predict(s))===Math.sign(s.score)).length/data.length};
  };
  const report={label:'pre_update_finite_search_proxy_not_stockfish_truth',raw:metric(samples,s=>s.raw),corrected:metric(samples,s=>s.corrected)};
  for(const phase of ['middlegame','endgame']) { const data=samples.filter(s=>(s.pieces<=10?'endgame':'middlegame')===phase); report[phase]={raw:metric(data,s=>s.raw),corrected:metric(data,s=>s.corrected)}; }
  report.component_only=Array.from({length:5},(_,i)=>metric(samples,s=>s.raw+Math.trunc(s.components_scaled16[i]/16)));
  return report;
}
const direct=process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url);
if(direct) {
  const o={engine:'',fens:'',out:'',limit:20,nodes:20000,depth:30,time:0,matrix:'ablation',profile:0};
  for(let i=2;i<process.argv.length;i+=2) { const key=process.argv[i].replace(/^--/,''); if(!(key in o) || !process.argv[i+1]) throw Error(`Invalid option ${key}`); o[key]=typeof o[key]==='number'?Number(process.argv[i+1]):process.argv[i+1]; }
  for(const k of ['engine','fens','out']) if(!o[k]) throw Error(`--${k} is required`);
  for(const k of ['limit','nodes','depth','time','profile']) if(!Number.isInteger(o[k]) || o[k]<0) throw Error(`Invalid ${k}`);
  if(!o.limit || !o.depth) throw Error('Positive limit/depth required');
  const all=['correction','continuation','capture_history','countermove','singular','verified_null','probcut','dynamic_lmr','history_pruning','see_pruning','mate_distance','iid','tt_policy','time_management','strategic_eval','king_safety','search_safety','pawn_cache'];
  const variants=[{name:'all_on_rfp_off',on:all,off:[]}];
  if(o.matrix==='ablation') for(const f of ['probcut','singular','iid','history_pruning','see_pruning','correction','continuation','dynamic_lmr','search_safety']) variants.push({name:`minus_${f}`,on:all,off:[f]});
  else if(o.matrix==='interaction') for(const [a,b] of [['dynamic_lmr','history_pruning'],['probcut','see_pruning'],['verified_null','rfp'],['correction','rfp'],['singular','dynamic_lmr'],['continuation','dynamic_lmr']]) {
    for(const mask of [0,1,2,3]) variants.push({name:`${a}_${b}_${mask}`,on:[...all.filter(x=>x!==a && x!==b),...(mask&1?[a]:[]),...(mask&2?[b]:[])],off:[a,b].filter((_,i)=>!(mask&(1<<i)))});
  } else if(o.matrix==='candidates') {
    for(const f of ['trend_safety','calibrated_lmr','adaptive_time','rfp']) variants.push({name:`plus_${f}`,on:[...all,f],off:[]});
  } else if(o.matrix==='correction') for(let i=0;i<5;++i) variants.push({name:`correction_minus_${i}`,on:all,off:[],mask:31^(1<<i)});
  else if(o.matrix!=='single') throw Error('Unknown matrix');
  const corpus=parseCorpus(await readFile(o.fens,'utf8'),o.fens.endsWith('.jsonl')).slice(0,o.limit);
  await mkdir(o.out); // Deliberately refuses to reuse a result directory.
  const hash=path=>readFile(path).then(b=>createHash('sha256').update(b).digest('hex'));
  await writeFile(resolve(o.out,'manifest.json'),JSON.stringify({options:o,variants,engine_sha256:await hash(o.engine),corpus_sha256:await hash(o.fens),created:new Date().toISOString()},null,2));
  const rows=[];
  for(let i=0;i<corpus.length;++i) for(const v of variants) {
    const args=['--analyze','--fen',corpus[i].fen,'--nodes',String(o.nodes),'--depth',String(o.depth),'--time',String(o.time),'--proof-nodes','0','--calibration-samples','256'];
    if(o.profile) args.push('--profile');
    if(v.mask!=null) args.push('--correction-mask',String(v.mask));
    for(const f of v.on) args.push('--feature',f);
    for(const f of v.off) args.push('--disable-feature',f);
    const run=spawnSync(resolve(o.engine),args,{encoding:'utf8',windowsHide:true,timeout:120000,maxBuffer:16*1024*1024});
    if(run.status!==0) throw Error(run.stderr||String(run.error)||'Engine failed');
    const result=JSON.parse(run.stdout); rows.push({position:i,variant:v.name,reference_move:corpus[i].reference_move??null,result});
    await writeFile(resolve(o.out,`${i}-${v.name}.json`),JSON.stringify(result));
  }
  const summaries=variants.map(v=>{
    const own=rows.filter(r=>r.variant===v.name),base=rows.filter(r=>r.variant===variants[0].name);
    const total=field=>own.reduce((n,r)=>n+r.result[field],0);
    const stats={threads:1}; for(const r of own) for(const [k,value] of Object.entries(r.result.search_stats)) if(k!=='threads' && k!=='root_correction') stats[k]=(stats[k]??0)+value;
    stats.root_correction_mean=own.reduce((n,r)=>n+r.result.search_stats.root_correction,0)/own.length;
    return {variant:v.name,positions:own.length,nodes:total('nodes'),elapsed_ms:total('elapsed_ms'),nps:Math.round(total('nodes')*1000/Math.max(1,total('elapsed_ms'))),mean_depth:total('depth')/own.length,agreement:own.filter((r,i)=>r.result.bestmove===base[i].result.bestmove).length/own.length,score_difference:own.map((r,i)=>r.result.score_cp-base[i].result.score_cp),reference_agreement:own.filter(r=>r.reference_move && r.reference_move===r.result.bestmove).length,reference_positions:own.filter(r=>r.reference_move).length,stats,correction:quality(own.flatMap(r=>r.result.correction_samples??[]))};
  });
  await writeFile(resolve(o.out,'summary.json'),JSON.stringify({kind:o.time?'fixed_time_search_not_elo':o.nodes?'node_bounded_not_elo':'fixed_depth_search_not_elo',summaries},null,2));
  console.log(`Saved ${rows.length} observations: ${o.out}`);
}
