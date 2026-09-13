import {spawn} from 'node:child_process';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {sha} from './causal-analysis.mjs';
import {Uci} from './stockfish-regression.mjs';
const o={engine:'baseline/v06-attribution/v06-attribution.exe',corpus:'results/v06-critical-corpus/critical.jsonl',reference:'reference/stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe',out:'',nodes:20000,limit:300,workers:4,reference_depth:18};
for(let i=2;i<process.argv.length;i+=2) {const k=process.argv[i].slice(2);if(!(k in o)) throw Error(k);o[k]=typeof o[k]==='number'?Number(process.argv[i+1]):process.argv[i+1];}
if(!o.out) throw Error('--out required');
for(const k of ['nodes','limit','workers','reference_depth']) if(!Number.isInteger(o[k]) || o[k]<1) throw Error(`Invalid ${k}`);
if(o.workers>4) throw Error('At most four controlled search workers');
const rows=(await readFile(o.corpus,'utf8')).trim().split(/\r?\n/).map(JSON.parse).slice(0,o.limit);
const masks=['legacy','all','no-improving','no-tactical','no-pv','no-king','no-onlymove','no-strategic','no-trend'];
await mkdir(o.out);
const save=(name,value)=>writeFile(resolve(o.out,name),JSON.stringify(value,null,2),{flag:'wx'});
await save('manifest.json',{options:o,date:new Date().toISOString(),command:process.argv,engine_sha256:await sha(o.engine),corpus_sha256:await sha(o.corpus),stockfish_sha256:await sha(o.reference),source_sha256:await sha('baseline/v06-attribution/v06-attribution-source.zip'),tool_sha256:await sha(new URL(import.meta.url)),compiler:'MSVC 19.51 Release AVX2 IPO',uci:{Threads:1,Hash:32,Middlegame:true,ProofNodes:0},concurrency:o.workers,interpretation:'Fixed-node search study; concurrent wall times are not CPU speed benchmarks. Reference is freshly regenerated per position at fixed depth.'});
function search(row,mask) {return new Promise((accept,reject)=>{
  const p=spawn(resolve(o.engine),['--analyze','--fen',row.fen,'--middlegame','--proof-nodes','0','--depth','100','--time','0','--nodes',String(o.nodes),'--safety-mask',mask],{windowsHide:true,stdio:['ignore','pipe','pipe']});
  let out='',err='';p.stdout.on('data',d=>out+=d);p.stderr.on('data',d=>err+=d);p.on('error',reject);
  const timer=setTimeout(()=>{p.kill();reject(Error('Search timeout'));},120000);
  p.on('close',code=>{clearTimeout(timer);if(code) reject(Error(err));else {try{accept(JSON.parse(out));}catch(e){reject(e);}}});
});}
const jobs=rows.flatMap(row=>masks.map(mask=>({row,mask}))),results=new Map();let index=0,done=0;
await Promise.all(Array.from({length:o.workers},async()=>{while(index<jobs.length){const {row,mask}=jobs[index++];const result=await search(row,mask);await save(`search-${row.id}-${mask}.json`,result);results.set(`${row.id}:${mask}`,result);if(++done%100===0) console.log(`search ${done}/${jobs.length}`);}}));
const sf=new Uci(resolve(o.reference)),references=new Map();
try {
  await sf.init({Threads:1,Hash:32,MultiPV:3});
  for(const row of rows) {
    const reference=await sf.search(row.fen,{depth:o.reference_depth,multipv:3,reference:true});
    const moves=new Set(masks.map(mask=>results.get(`${row.id}:${mask}`).bestmove)),forced={};
    for(const move of moves) {
      if(move==='0000') continue;
      forced[move]=await sf.search(row.fen,{depth:o.reference_depth,moves:[move],reference:true});
    }
    const entry={reference,forced};references.set(row.id,entry);await save(`reference-${row.id}.json`,entry);
    if(references.size%25===0) console.log(`reference ${references.size}/${rows.length}`);
  }
} finally {sf.close();}
const report=[];
for(const mask of masks) {
  const observations=rows.map(row=>{
    const r=results.get(`${row.id}:${mask}`),ref=references.get(row.id),best=ref.reference.rows[0],played=ref.forced[r.bestmove]?.rows[0];
    const cp=best?.type==='cp' && played?.type==='cp' && best.depth>=o.reference_depth && played.depth>=o.reference_depth;
    const top=ref.reference.rows.map(r=>r.pv[0]),raw=cp?best.score-played.score:null;
    const rank=r.moves.findIndex(m=>m.move===ref.reference.bestmove);
    return {id:row.id,move:r.bestmove,reference:ref.reference.bestmove,top1:r.bestmove===ref.reference.bestmove,top3:top.includes(r.bestmove),raw_cp_loss:raw,cp_loss:cp?Math.max(0,raw):null,depth:r.depth,nodes:r.nodes,qnodes:r.search_stats.qnodes,lmr_events:r.search_stats.lmr_reductions,root_rank:rank>=0?rank+1:null,root_rank_is_bound_order:true,mate_or_missing:!cp};
  });
  await save(`metrics-${mask}.json`,observations);
  const losses=observations.map(r=>r.cp_loss).filter(x=>x!==null).sort((a,b)=>a-b),n=losses.length;
  report.push({mask,n:rows.length,top1:observations.filter(r=>r.top1).length,top3:observations.filter(r=>r.top3).length,cp_positions:n,mean_cp_loss:n?losses.reduce((a,b)=>a+b,0)/n:null,median_cp_loss:n?(losses[Math.floor((n-1)/2)]+losses[Math.floor(n/2)])/2:null,p95_cp_loss:n?losses[Math.ceil(n*.95)-1]:null,max_cp_loss:n?losses.at(-1):null,blunders_gt200:losses.filter(v=>v>200).length,major_gt100:losses.filter(v=>v>100).length,average_depth:observations.reduce((a,b)=>a+b.depth,0)/rows.length});
}
await save('summary.json',{results:report,interpretation:'Reference-estimated CP loss; negative raw differences are retained and clamped only for loss metrics. MultiPV and forced-root search disagree in some positions. No causal or Elo conclusion.'});
