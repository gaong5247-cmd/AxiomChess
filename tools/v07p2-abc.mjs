import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {search,sha} from './causal-analysis.mjs';
import {Uci} from './stockfish-regression.mjs';
const out=process.argv[2];if(!out)throw Error('output directory');await mkdir(out);
const read=async p=>JSON.parse(await readFile(p,'utf8')),save=(n,d)=>writeFile(`${out}/${n}.json`,JSON.stringify(d,null,2),{flag:'wx'});
const fens=(await read('results/v07p2-benchmark/manifest.json')).corpus;
const variants=[{name:'A',exe:'baseline/v06-final.exe',extra:[]},{name:'B',exe:'baseline/v07-phase1.exe',extra:['--feature','reuse_move_facts']},{name:'C2',exe:'baseline/v07-phase2.exe',extra:['--feature','legal_fast_path']}];
await save('manifest',{date:new Date().toISOString(),command:process.argv,compiler:'MSVC Release AVX2 IPO',threads:1,hash_mb:32,proof_nodes:0,features:'Middlegame + isolated variant flag',variants:await Promise.all(variants.map(async v=>({...v,sha256:await sha(v.exe)}))),fens,repetitions:3,limits:{nodes:20000,depth:4,time_ms:100},order:'Rotated A/B/C per repetition; no concurrent intentional benchmark workload'});
for(const v of variants)search(v.exe,fens[0],['--nodes','2000',...v.extra]);
const observations=[];
for(const mode of ['nodes','depth','time'])for(let rep=0;rep<3;++rep)for(let index=0;index<fens.length;++index)for(let j=0;j<3;++j){
 const v=variants[(j+rep)%3],limit=mode==='nodes'?['--depth','100','--nodes','20000']:mode==='depth'?['--depth','4']:['--depth','100','--time','100'];
 observations.push({variant:v.name,mode,rep,index,result:search(v.exe,fens[index],[...limit,...v.extra])});
}
await save('observations',observations);
const summary=[];
for(const mode of ['nodes','depth','time'])for(const v of variants){const rows=observations.filter(r=>r.mode===mode&&r.variant===v.name),totals=[0,1,2].map(rep=>rows.filter(r=>r.rep===rep).reduce((n,r)=>n+r.result.elapsed_ms,0));
summary.push({mode,variant:v.name,elapsed_per_repeat:totals,median_elapsed_ms:totals.toSorted((a,b)=>a-b)[1],average_depth:rows.reduce((n,r)=>n+r.result.depth,0)/rows.length,nodes:rows.reduce((n,r)=>n+r.result.nodes,0),qnodes:rows.reduce((n,r)=>n+r.result.search_stats.qnodes,0)});}
await save('summary',summary);console.log('A/B/C repeated timings complete');
const refs=await read('results/v07p2-benchmark/reference.json'),sfexe='reference/stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe',sf=new Uci(resolve(sfexe));
try{await sf.init({Threads:1,Hash:32,MultiPV:1});for(const r of observations.filter(r=>r.mode==='time')){let ref=refs[r.index];if(!ref.forced[r.result.bestmove])ref.forced[r.result.bestmove]=await sf.search(fens[r.index],{depth:18,moves:[r.result.bestmove],reference:true});}}finally{sf.close();}
await save('references',{stockfish_sha256:await sha(sfexe),source_bestmove_reference:'results/v07p2-benchmark/reference.json',refs});
const quality=variants.map(v=>{const rows=observations.filter(r=>r.variant===v.name&&r.mode==='time');let top1=0,top3=0;const losses=[];
for(const r of rows){let ref=refs[r.index],b=ref.ref.rows[0],p=ref.forced[r.result.bestmove]?.rows[0];top1+=r.result.bestmove===ref.ref.bestmove;top3+=ref.ref.rows.some(x=>x.pv[0]===r.result.bestmove);if(b?.type==='cp'&&p?.type==='cp')losses.push(Math.max(0,b.score-p.score));}
return{variant:v.name,positions:fens.length,observations:rows.length,top1,top3,cp_samples:losses.length,mean_cp_loss:losses.reduce((a,b)=>a+b,0)/losses.length,above_cp:Object.fromEntries([50,100,200,400].map(t=>[t,losses.filter(x=>x>t).length])),interpretation:'Repeated observations are not independent; mate outcomes unclassified, not Elo.'};});
await save('quality',quality);console.log('A/B/C reference comparison complete');
