import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {search,sha} from './causal-analysis.mjs';
import {Uci} from './stockfish-regression.mjs';
const exe=process.argv[2]??'build/Release/axiom-0.7-cost.exe',out=process.argv[3];
const feature=process.argv[4]??'reuse_move_facts';
if(!out) throw Error('engine output-directory required');
await mkdir(out);const save=(name,data)=>writeFile(resolve(out,name),JSON.stringify(data,null,2),{flag:'wx'});
const bench=(await readFile('tests/bench.fens','utf8')).split(/\r?\n/).filter(s=>s && !s.startsWith('#'));
const critical=(await readFile('results/v06-critical-corpus/critical.jsonl','utf8')).trim().split(/\r?\n/).map(JSON.parse).slice(0,15).map(r=>r.fen);
const corpus=[...new Set([...bench,...critical])],observations=[];
const sfexe='reference/stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe';
await save('manifest.json',{command:process.argv,date:new Date().toISOString(),engine_sha256:await sha(exe),tool_sha256:await sha(new URL(import.meta.url)),stockfish_sha256:await sha(sfexe),compiler:'MSVC 19.51 Release AVX2 IPO; tracing compiled out',threads:1,hash_mb:32,corpus,features:`Middlegame; only ${feature} differs`,repeats:3,order:'alternating OFF/ON per repetition',limits:{nodes:20000,depth:4,time_ms:100},reference_depth:18,interpretation:'Same binary feature ablation. No other benchmark jobs deliberately run concurrently.'});
// Warm the executable/runtime without including this sample in the report.
for(const enabled of [false,true]) search(exe,corpus[0],['--nodes','2000',...(enabled?['--feature',feature]:[])]);
for(const mode of ['nodes','depth','time']) for(let repeat=0;repeat<3;++repeat) for(let index=0;index<corpus.length;++index) for(const enabled of repeat%2?[true,false]:[false,true]) {
  const limit=mode==='nodes'?['--depth','100','--nodes','20000']:mode==='depth'?['--depth','4']:['--depth','100','--time','100'];
  const result=search(exe,corpus[index],[...limit,...(enabled?['--feature',feature]:[])]);
  observations.push({mode,repeat,index,enabled,result});
}
await save('observations.json',observations);
const median=v=>v.toSorted((a,b)=>a-b)[Math.floor(v.length/2)];
const summary=[];
for(const mode of ['nodes','depth','time']) for(const enabled of [false,true]) {
  const samples=observations.filter(r=>r.mode===mode && r.enabled===enabled),totals=[0,1,2].map(repeat=>samples.filter(s=>s.repeat===repeat).reduce((a,s)=>a+s.result.elapsed_ms,0));
  summary.push({mode,enabled,positions:corpus.length,repeats:3,elapsed_ms_per_repeat:totals,median_elapsed_ms:median(totals),average_depth:samples.reduce((a,s)=>a+s.result.depth,0)/samples.length,total_nodes:samples.reduce((a,s)=>a+s.result.nodes,0),qnodes:samples.reduce((a,s)=>a+s.result.search_stats.qnodes,0),max_overshoot_ms:Math.max(...samples.map(s=>s.result.time_telemetry.hard_overshoot_ms))});
}
await save('summary.json',summary);console.log('Completed repeated fixed-node/depth/time benchmark; reference evaluation follows');
const sf=new Uci(resolve(sfexe)),references=[];
try {
  await sf.init({Threads:1,Hash:32,MultiPV:3});
  for(let index=0;index<corpus.length;++index) {
    const moves=new Set(observations.filter(o=>o.mode==='time' && o.index===index).map(o=>o.result.bestmove));
    const ref=await sf.search(corpus[index],{depth:18,multipv:3,reference:true}),forced={};
    for(const move of moves) if(move!=='0000') forced[move]=await sf.search(corpus[index],{depth:18,moves:[move],reference:true});
    references.push({index,ref,forced});
  }
} finally {sf.close();}
await save('reference.json',references);
const quality=[false,true].map(enabled=>{
  const samples=observations.filter(o=>o.mode==='time' && o.enabled===enabled);let top1=0,top3=0;const losses=[];
  for(const s of samples) {const ref=references[s.index],best=ref.ref.rows[0],chosen=ref.forced[s.result.bestmove]?.rows[0];top1+=s.result.bestmove===ref.ref.bestmove;top3+=ref.ref.rows.some(r=>r.pv[0]===s.result.bestmove);if(best?.type==='cp' && chosen?.type==='cp')losses.push(Math.max(0,best.score-chosen.score));}
  return {enabled,searches:samples.length,unique_positions:corpus.length,top1,top3,cp_positions:losses.length,mean_cp_loss:losses.reduce((a,b)=>a+b,0)/Math.max(1,losses.length),blunders_gt200:losses.filter(v=>v>200).length,interpretation:'Repeated observations are not independent positions; not Elo.'};
});
await save('fixed-time-quality.json',quality);console.log('Completed Stockfish depth-18 fixed-time quality comparison');
