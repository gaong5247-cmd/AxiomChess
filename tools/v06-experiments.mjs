import {spawnSync} from 'node:child_process';
import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
const o={mode:'safety',engine:'build/Release/axiom-0.6-attribution.exe',corpus:'results/v06-critical-corpus/critical.jsonl',out:'',limit:30,nodes:20000};
for(let i=2;i<process.argv.length;i+=2) {
  const key=process.argv[i].slice(2); if(!(key in o) || process.argv[i+1]===undefined) throw Error(`Invalid argument ${key}`);
  o[key]=typeof o[key]==='number'?Number(process.argv[i+1]):process.argv[i+1];
}
if(!o.out || !Number.isInteger(o.limit) || o.limit<1 || !Number.isInteger(o.nodes) || o.nodes<1) throw Error('Specify --out and positive limit/nodes');
const hash=data=>createHash('sha256').update(data).digest('hex');
const filehash=async path=>hash(await readFile(path));
const records=(await readFile(o.corpus,'utf8')).trim().split(/\r?\n/).map(JSON.parse).slice(0,o.limit);
await mkdir(o.out); // Never overwrite an experiment.
const save=async (name,data)=>writeFile(resolve(o.out,name),JSON.stringify(data,null,2),{flag:'wx'});
const sources={};
for(const dir of ['src','include/axiom','tools']) for(const name of (await readdir(dir)).sort()) {
  if(/\.(cpp|hpp|mjs|ps1)$/.test(name)) sources[`${dir}/${name}`]=await filehash(`${dir}/${name}`);
}
await save('manifest.json',{options:o,command:process.argv,date:new Date().toISOString(),engine_sha256:await filehash(o.engine),source_revision:hash(JSON.stringify(sources)),sources,corpus_sha256:await filehash(o.corpus),threads:1,hash_mb:32,features:'Middlegame',proof_nodes:0,reference:'Frozen corpus Stockfish depth 18 scores; no fresh reference search',time_control:'fixed nodes unless row specifies depth/time',opening:'real-game corpus FEN roots; no opening book',interpretation:'Diagnostic experiment, not Elo or causal proof'});
function execute(exe,args) {
  const p=spawnSync(resolve(exe),args,{encoding:'utf8',windowsHide:true,maxBuffer:64*1024*1024,timeout:120000});
  if(p.status!==0) throw Error(p.stderr||String(p.error)); return JSON.parse(p.stdout);
}
function analyze(row,extra=[],budget=['--depth','100','--nodes',String(o.nodes)]) {
  return execute(o.engine,['--analyze','--fen',row.fen,'--time','0','--proof-nodes','0','--middlegame',...budget,...extra]);
}
function metrics(values) {
  const a=values.map(Math.abs).sort((x,y)=>x-y),n=a.length;
  const q=p=>a[Math.max(0,Math.ceil(n*p)-1)];
  return {n,mae:a.reduce((x,y)=>x+y,0)/n,rmse:Math.sqrt(a.reduce((x,y)=>x+y*y,0)/n),median:(a[Math.floor((n-1)/2)]+a[Math.floor(n/2)])/2,p75:q(.75),p90:q(.9),p95:q(.95)};
}
if(o.mode==='holdout') {
  await writeFile(resolve(o.out,'holdout.fens'),records.map(r=>r.fen).join('\n')+'\n',{flag:'wx'});
  const trained=execute('build/Release/axiom_holdout.exe',['tests/middlegame-balanced.fens',resolve(o.out,'holdout.fens')]);
  await save('frozen-state-and-samples.json',trained);
  const masks=[0,31,30,29,27,23,15,1,2,4,8,16],summary=[];
  for(const mask of masks) {
    const rows=trained.samples.map((s,i)=>{
      const selected=s.components_scaled16.filter((_,j)=>mask & (1<<j));
      const shift=selected.length?Math.trunc(selected.reduce((a,b)=>a+b,0)/(16*selected.length)):0;
      const prediction=Math.max(-28999,Math.min(28999,s.raw+shift));
      return {id:records[i].id,fen:s.fen,phase:records[i].phase,raw:s.raw,shift,prediction,target:records[i].reference_score_before,error:prediction-records[i].reference_score_before};
    });
    await save(`mask-${mask}.json`,rows);
    const grade=rs=>({...metrics(rs.map(r=>r.error)),sign_agreement:rs.filter(r=>Math.sign(r.prediction)===Math.sign(r.target)).length/rs.length});
    const phases={}; for(const phase of new Set(rows.map(r=>r.phase))) { const group=rows.filter(r=>r.phase===phase); phases[phase]=group.length>=30?grade(group):{n:group.length,insufficient:true}; }
    summary.push({mask,...grade(rows),phases});
  }
  await save('summary.json',{state_unchanged:trained.state_unchanged,state_sha256:hash(JSON.stringify(trained.state_scaled16)),snapshot_binary_sha256:await filehash('build/Release/axiom_holdout.exe'),training_file_sha256:await filehash('tests/middlegame-balanced.fens'),training_roots:trained.training_roots,previous_token:0,limitations:'Disjoint training roots, not guaranteed disjoint search descendants. Root previous token is unavailable: prevmove component is evaluated at sentinel 0. Leave-one-out averages remaining tables; it is not an additive contribution decomposition. Frozen reference targets generated before component comparison.',masks:summary});
} else if(o.mode==='attribution') {
  const variants=[['LMR_OVER_REDUCTION',['--no-lmr']],['NULL_MOVE',['--no-null']],['CORRECTION_BIAS',['--disable-feature','correction']],['KING_SAFETY',['--disable-feature','king_safety']],['MOVE_ORDERING',['--partial-ordering']],['TT_CONTEXT',['--no-tt']],['SEARCH_SAFETY',['--safety-mask','none']],['HISTORY_PRUNING',['--disable-feature','history_pruning']],['SEE_PRUNING',['--disable-feature','see_pruning']],['PROBCUT',['--disable-feature','probcut']],['SINGULAR_PATH',['--disable-feature','singular']]];
  const output=[];
  for(const row of records) {
    const base=analyze(row,['--calibration-samples','256']),selected=analyze(row,['--force-root-move',base.bestmove]),reference=analyze(row,['--force-root-move',row.reference_move]);
    const evidence=[];
    for(const [category,args] of variants) {
      const inactive=['HISTORY_PRUNING','SEE_PRUNING','PROBCUT','SINGULAR_PATH'].includes(category);
      const replay=analyze(row,args); evidence.push({category,baseline_inactive:inactive,command_flags:args,bestmove:replay.bestmove,score:replay.score_cp,depth:replay.depth,nodes:replay.nodes,changed:replay.bestmove!==base.bestmove,reference_recovered:!inactive && base.bestmove!==row.reference_move && replay.bestmove===row.reference_move,stats:replay.search_stats});
    }
    const recovered=evidence.filter(e=>e.reference_recovered).map(e=>e.category);
    const entry={id:row.id,fen:row.fen,historical_move:row.axiom_move,historical_cp_loss:row.cp_loss,reference_move:row.reference_move,current_move:base.bestmove,baseline:base,forced_selected:selected,forced_reference:reference,evidence,likely_cause:recovered.length===1?recovered[0]:'UNKNOWN',confidence:'SINGLE_CONTROLLED_REPLAY_NOT_CAUSAL',suspects:recovered,limitations:'Current replays do not reconstruct original game TT/history. Equal node budget can change completed depth. Historical CP loss is not current-move CP loss. No invented confidence probability.'};
    await save(`position-${row.id}.json`,entry); output.push({id:row.id,likely_cause:entry.likely_cause,suspects:recovered});
  }
  await save('summary.json',{n:output.length,rows:output,unattributed_categories:['STATIC_EVAL','PAWN_STRUCTURE','HORIZON','QSEARCH','TIME_MANAGEMENT','FUTILITY'],limitations:'Unattributed categories require additional targeted experiments. Disabled heuristics are negative controls, not causal evidence.'});
} else if(o.mode==='time') {
  const rows=[];
  for(const budget of [1,5,20,50]) for(const guard of [false,true]) for(const row of records) {
    const result=analyze(row,guard?['--time-guard']:[],['--depth','100','--time',String(budget)]);
    rows.push({id:row.id,budget_ms:budget,guard,result});
  }
  await save('observations.json',rows);
  await save('summary.json',[false,true].map(guard=>{const group=rows.filter(r=>r.guard===guard); return {guard,n:group.length,guard_stops:group.filter(r=>r.result.time_telemetry.guard_stopped).length,max_engine_overshoot_ms:Math.max(...group.map(r=>r.result.time_telemetry.hard_overshoot_ms)),limitations:'CLI deadline smoke, not UCI game-clock forfeits. Does not isolate OS scheduling or individual long-operation stalls.'};}));
} else {
  const reasons=['tt','pv','history','continuation','counter','check','promotion','unstable','king','singular','onlymove','strategic','tactical','improving'];
  let variants;
  if(o.mode==='safety') variants=[['legacy',['--safety-mask','legacy']],['all',['--safety-mask','all']],...reasons.map(r=>[`no-${r}`,['--safety-mask',`no-${r}`]])];
  else if(o.mode==='ordering') variants=[['full',[]],['partial',['--partial-ordering']]];
  else if(o.mode==='profile') variants=[['profile',['--profile']]];
  else throw Error(`Unknown mode ${o.mode}`);
  const summary=[];
  const budgets=o.mode==='ordering'?[['nodes',['--depth','100','--nodes',String(o.nodes)]],['depth',['--depth','4']],['time',['--depth','100','--time','50']]]:[['nodes',['--depth','100','--nodes',String(o.nodes)]]];
  for(const [budget,args] of budgets) for(const [name,flags] of variants) {
    const rows=records.map(row=>({id:row.id,reference:row.reference_move,result:analyze(row,flags,args)}));
    await save(`${budget}-${name}.json`,rows);
    summary.push({budget,name,positions:rows.length,agreement:rows.filter(r=>r.result.bestmove===r.reference).length,average_depth:rows.reduce((a,r)=>a+r.result.depth,0)/rows.length,nodes:rows.reduce((a,r)=>a+r.result.nodes,0),elapsed_ms:rows.reduce((a,r)=>a+r.result.elapsed_ms,0),q_explosion_flags:rows.filter(r=>r.result.qsearch_ratio>.95).map(r=>r.id),interpretation:'Q ratio > .95 is a profiling flag, not a correctness failure.'});
    console.log(`${o.mode} ${budget} ${name} complete`);
  }
  await save('summary.json',summary);
}
console.log(`Completed ${o.mode}: ${records.length} positions`);
