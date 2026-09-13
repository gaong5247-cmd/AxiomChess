import {spawnSync} from 'node:child_process';
import {readFile,writeFile,appendFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {Uci} from './stockfish-regression.mjs';
const o={pgn:'',converter:'',reference:'',out:'',target:300,depth:18,screen:10000};
for(let i=2;i<process.argv.length;i+=2) { const k=process.argv[i].slice(2); if(!(k in o)) throw Error(`Unknown ${k}`); o[k]=typeof o[k]==='number'?Number(process.argv[i+1]):process.argv[i+1]; }
for(const k of ['pgn','converter','reference','out']) if(!o[k]) throw Error(`--${k} required`);
for(const k of ['target','depth','screen']) if(!Number.isInteger(o[k]) || o[k]<1) throw Error(`Invalid ${k}`);
const paths=o.pgn.split('|'),groups=[];
const hash=async p=>createHash('sha256').update(await readFile(p)).digest('hex');
for(const path of paths) {
  const replay=spawnSync(resolve(o.converter),[resolve(path)],{encoding:'utf8',windowsHide:true,maxBuffer:64*1024*1024});
  if(replay.status!==0) throw Error(replay.stderr||String(replay.error));
  const games=new Map();
  for(const line of replay.stdout.trim().split(/\r?\n/)) if(line) { const p=JSON.parse(line); (games.get(p.game)??games.set(p.game,[]).get(p.game)).push({...p,source:resolve(path)}); }
  groups.push(...games.values());
}
await mkdir(o.out);
await writeFile(resolve(o.out,'manifest.json'),JSON.stringify({options:o,source_revision:'v05-frozen-games/v06-extraction-tool',command_line:process.argv,uci:{Threads:1,Hash:32},reference_sha256:await hash(o.reference),converter_sha256:await hash(o.converter),tool_sha256:await hash(new URL(import.meta.url)),pgn:await Promise.all(paths.map(async path=>({path,sha256:await hash(path)}))),date:new Date().toISOString(),policy:'Low-node screening, depth-qualified reference confirmation. First/largest/crossings mean observed qualifying positions, not exhaustive game oracle.'},null,2));
const ref=new Uci(resolve(o.reference)); const seen=new Set(),done=new Set(),previous=new Map();
const records=[]; let screened=0,confirmed=0,excluded=0;
const stability=result=>{
  const byDepth=new Map(result.iterations.filter(r=>r.multipv===1).map(r=>[r.depth,r]));
  const last=[...byDepth.values()].sort((a,b)=>a.depth-b.depth).slice(-3);
  const scores=last.filter(r=>r.type==='cp').map(r=>r.score),mean=scores.reduce((a,b)=>a+b,0)/Math.max(1,scores.length);
  const variance=scores.reduce((a,b)=>a+(b-mean)**2,0)/Math.max(1,scores.length);
  return {depths:last.map(r=>r.depth),moves:last.map(r=>r.pv[0]),scores:last.map(r=>({type:r.type,value:r.score})),variance,last_three_same:last.length===3 && new Set(last.map(r=>r.pv[0])).size===1,grade:last.length===3 && new Set(last.map(r=>r.pv[0])).size===1 && scores.length===3 && variance<=400?'HIGH':'LOW'};
};
try {
  await ref.init({Threads:1,Hash:32});
  const maxPly=Math.max(...groups.map(g=>g.length));
  outer: for(let index=0;index<maxPly;++index) for(let game=0;game<groups.length;++game) {
    const p=groups[game][index]; if(!p || p.repetitions>=2) continue;
    const key=`${game}:${p.side}`; if(done.has(key) || seen.has(p.fen)) continue;
    const screen=await ref.search(p.fen,{nodes:o.screen,reference:true}); ++screened;
    if(screen.bestmove===p.move) continue;
    const forcedScreen=await ref.search(p.fen,{nodes:o.screen,reference:true,moves:[p.move]});
    if(screen.rows[0]?.type!=='cp' || forcedScreen.rows[0]?.type!=='cp' || screen.rows[0].score-forcedScreen.rows[0].score<20) continue;
    const before=await ref.search(p.fen,{depth:o.depth,reference:true});
    if(before.bestmove===p.move) continue;
    const after=await ref.search(p.fen,{depth:o.depth,reference:true,moves:[p.move]}); ++confirmed;
    const a=before.rows[0],b=after.rows[0];
    if(a?.type!=='cp' || b?.type!=='cp' || a.depth<o.depth || b.depth<o.depth) { ++excluded; continue; }
    const loss=Math.max(0,a.score-b.score); if(loss<30) continue;
    const thresholds=[30,50,80,100,150,200].filter(t=>loss>=t);
    const record={...p,id:records.length,game_id:game,axiom_move:p.move,axiom_score:null,axiom_depth:null,axiom_pv:null,reference_move:before.bestmove,reference_score_before:a.score,reference_score_after:b.score,reference_depth:o.depth,cp_loss:loss,raw_cp_loss:a.score-b.score,thresholds,reference_stability:stability(before),category:'UNKNOWN',first_observed_critical:!previous.has(key),previous_observed_loss:previous.get(key)??null,screen_nodes:o.screen};
    seen.add(p.fen); previous.set(key,loss); records.push(record); done.add(key);
    await appendFile(resolve(o.out,'critical.jsonl'),JSON.stringify(record)+'\n');
    await writeFile(resolve(o.out,'progress.json'),JSON.stringify({screened,confirmed,excluded,critical:records.length,target:o.target}));
    if(records.length%25===0) console.log(`critical=${records.length} screened=${screened} confirmed=${confirmed}`);
    if(records.length>=o.target) break outer;
  }
} finally {ref.close();}
await writeFile(resolve(o.out,'selection-status.json'),JSON.stringify({selection:'FIRST_OBSERVED_QUALIFYING_PER_GAME_SIDE',exhaustive:false,first_actual_mistake:false,largest_mistake:false,all_threshold_crossings:false,reason:'Screening can miss earlier errors; subsequent moves of a selected game-side were not analyzed.'},null,2));
await writeFile(resolve(o.out,'summary.json'),JSON.stringify({critical:records.length,unique:seen.size,screened,confirmed,excluded,high_reference_stability:records.filter(r=>r.reference_stability.grade==='HIGH').length,target_met:records.length>=o.target,complete:true},null,2));
console.log(`Completed ${records.length} unique critical FENs`);
