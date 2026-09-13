import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {search,sha,analyzeTrace} from './causal-analysis.mjs';
const o={engine:'build-causal/Release/axiom-0.6-causal-research.exe',corpus:'results/v06-critical-corpus/critical.jsonl',out:'',limit:300,nodes:20000,events:5000};
for(let i=2;i<process.argv.length;i+=2) {const k=process.argv[i].slice(2);if(!(k in o)) throw Error(k);o[k]=typeof o[k]==='number'?Number(process.argv[i+1]):process.argv[i+1];}
if(!o.out) throw Error('--out required');
for(const k of ['limit','nodes','events']) if(!Number.isInteger(o[k]) || o[k]<1) throw Error(`Invalid ${k}`);
await mkdir(o.out);
const save=(name,data)=>writeFile(resolve(o.out,name),JSON.stringify(data,null,2),{flag:'wx'});
const rows=(await readFile(o.corpus,'utf8')).trim().split(/\r?\n/).map(JSON.parse).slice(0,o.limit);
await save('manifest.json',{date:new Date().toISOString(),command:process.argv,options:o,engine_sha256:await sha(o.engine),corpus_sha256:await sha(o.corpus),stockfish_sha256:await sha('reference/stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe'),compiler:'MSVC 19.51 Release C++20 AVX2 IPO research tracing',uci:{Middlegame:true,Hash:32,Threads:1,ProofNodes:0,SafetyMask:'legacy'},reference:'Historical corpus reference, not independently restabilized for this trace run'});
const summary=[];
for(const row of rows) {
  const path=resolve(o.out,`trace-${row.id}.jsonl`);
  const result=search(o.engine,row.fen,['--depth','100','--nodes',String(o.nodes),'--trace-root-move',row.reference_move,'--trace-min-iteration','3','--trace-max-events',String(o.events),'--trace-output',path]);
  const events=(await readFile(path,'utf8')).trim().split(/\r?\n/).map(JSON.parse),report=analyzeTrace(events,row.reference_move);
  await save(`position-${row.id}.json`,{id:row.id,reference_stability:row.reference_stability,result,report});
  summary.push({id:row.id,category:report.category,evidence:report.evidence,trace_complete:report.trace_complete,reference_legal:report.reference_legal,root_searched:report.root_searched,first_loss_of_lead:report.first_loss_of_lead,lmr_events:report.reduction_events.length,prune_events:Object.entries(report.events).filter(([k])=>k.includes('PRUNE')).reduce((a,[,v])=>a+v,0),q_ratio:result.qsearch_ratio,max_recorded_ply:Math.max(0,...events.filter(e=>e.ply!==undefined).map(e=>e.ply))});
  if(summary.length%25===0) console.log(`Traced ${summary.length}/${rows.length}`);
}
await save('summary.json',{n:summary.length,rows:summary,categories:{UNKNOWN:summary.length},interpretation:'Direct observed events are not causal error categories. Missing capped events remain unknown.'});
await save('qsearch-worst.json',summary.toSorted((a,b)=>b.q_ratio-a.q_ratio).slice(0,30).map(s=>({...s,fen:rows.find(r=>r.id===s.id).fen})));
