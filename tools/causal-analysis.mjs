import {spawnSync} from 'node:child_process';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
export const sha=async path=>createHash('sha256').update(await readFile(path)).digest('hex');
export function analyzeTrace(events,reference) {
  const summary=events.find(e=>e.reason==='TRACE_SUMMARY'),meta=events.find(e=>e.reason==='TRACE_MANIFEST');
  const rows=events.filter(e=>e.reason==='ROOT_ITERATION_RANK' && e.move===reference);
  // Root scores may be bounds. Rank is the engine's reported candidate order,
  // not a MultiPV exact-score ranking of every legal move.
  const ranks=[...new Map(rows.map(e=>[e.depth,{depth:e.depth,rank:e.index+1,score:e.score,bound:e.tt_bound}])).values()];
  const branch=events.filter(e=>e.root_move===reference);
  const reasons={}; for(const e of branch) reasons[e.reason]=(reasons[e.reason]??0)+1;
  const matrix={joint:{},exclusive:{},combinations:{}};
  for(const e of branch.filter(e=>e.reason.endsWith('BLOCKED_BY_SAFETY'))) {
    const bits=Array.from({length:14},(_,i)=>1<<i).filter(b=>e.safety_reasons & b),key=String(e.safety_reasons);
    const value=matrix.combinations[key]??={events:0,prevented_reduction_plies:0}; ++value.events; value.prevented_reduction_plies+=e.reason==='LMR_BLOCKED_BY_SAFETY'?e.reduction:0;
    for(const b of bits) matrix.joint[b]=(matrix.joint[b]??0)+1;
    if(bits.length===1) matrix.exclusive[bits[0]]=(matrix.exclusive[bits[0]]??0)+1;
  }
  const reductions=branch.filter(e=>e.reason==='LMR_APPLIED');
  const directRecoveries=branch.filter(e=>e.reason==='LMR_FULL_DEPTH_RESULT' && e.score>e.alpha);
  return {root_fen:meta?.root_fen,reference,trace_complete:summary?.complete===true,dropped:summary?.dropped??null,
    reference_legal:rows.length || branch.some(e=>e.reason==='ROOT_SEARCH_BEGIN')?true:null,
    root_searched:branch.some(e=>e.reason==='ROOT_SEARCH_BEGIN')?true:null,
    root_order:branch.filter(e=>e.reason==='ROOT_SEARCH_BEGIN').map(e=>({depth:e.depth,index:e.index,node:e.node_id})),ranks,
    first_shallow_best:ranks.length?ranks[0].rank===1:null,
    first_loss_of_lead:ranks.find((r,i)=>i>0 && ranks[i-1].rank===1 && r.rank!==1)?.depth??null,
    events:reasons,safety_matrix:matrix,reduction_events:reductions.map(e=>({event_id:e.event_id,node_id:e.node_id,fen:e.fen,move:e.move,depth:e.depth,reduction:e.reduction,history:e.history})),
    direct_full_depth_researches_above_alpha:directRecoveries.length,
    evidence:reductions.length?'DIRECT_REDUCTION_EVENT':Object.keys(reasons).some(r=>r.includes('PRUNE'))?'DIRECT_PRUNE_EVENT':'UNKNOWN',
    category:'UNKNOWN',limitations:'A direct event proves the event occurred, not that it caused a root error. Truncated or unvisited branches are unknown, never absent. Frozen FEN replay does not restore original TT, feedback, repetition, window, excluded/synthetic state.'};
}
export function search(exe,fen,args=[]) {
  const result=spawnSync(resolve(exe),['--analyze','--fen',fen,'--middlegame','--proof-nodes','0','--time','0',...args],{encoding:'utf8',windowsHide:true,maxBuffer:32*1024*1024,timeout:120000});
  if(result.status!==0) throw Error(result.stderr||String(result.error)); return JSON.parse(result.stdout);
}
if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const o={trace:'',node:0,event:0,out:'',engine:'build-causal/Release/axiom-0.6-causal-research.exe',nodes:20000};
  for(let i=2;i<process.argv.length;i+=2) { const k=process.argv[i].slice(2); if(!(k in o)) throw Error(`Unknown ${k}`); o[k]=typeof o[k]==='number'?Number(process.argv[i+1]):process.argv[i+1]; }
  if(!o.trace || !o.out) throw Error('--trace and --out required');
  const events=(await readFile(o.trace,'utf8')).trim().split(/\r?\n/).map(JSON.parse),meta=events[0];
  await mkdir(o.out);
  const save=(name,value)=>writeFile(resolve(o.out,name),JSON.stringify(value,null,2),{flag:'wx'});
  await save('manifest.json',{date:new Date().toISOString(),command:process.argv,trace_sha256:await sha(o.trace),engine_sha256:await sha(o.engine),options:o});
  await save('disappearance.json',analyzeTrace(events,meta.root_filter));
  if(o.node || o.event) {
    const event=o.event?events.find(e=>e.event_id===o.event):events.find(e=>e.node_id===o.node && e.reason==='NODE_ENTER');
    if(!event?.fen || event.depth<1 || event.synthetic || event.auxiliary) throw Error('Select a recorded non-synthetic/non-auxiliary main-search node at depth >=1');
    const force=event.move?['--force-root-move',event.move]:[],baseArgs=['--nodes',String(o.nodes),'--depth',String(event.depth),'--replay-subtree','--replay-alpha',String(event.alpha),'--replay-beta',String(event.beta)];
    const variants=[['baseline',[]],['no-lmr',['--no-lmr']],['no-history',['--disable-feature','history_pruning']],['no-see-main',['--disable-feature','see_pruning']],['no-null',['--no-null']],['no-probcut',['--disable-feature','probcut']],['no-safety',['--safety-mask','none']]];
    const replay=[];
    for(const [name,args] of variants) { const runs=[search(o.engine,event.fen,[...baseArgs,...args]),search(o.engine,event.fen,[...baseArgs,...args])];
      const same=['bestmove','score_cp','depth','nodes'].every(k=>runs[0][k]===runs[1][k]); replay.push({name,args,reproducible:same,runs}); }
    if(event.reduction>0 && event.move) {
      const depthArgs=['--nodes',String(o.nodes),...force];
      replay.push({name:'original-reduced-depth-fresh-window',runs:[search(o.engine,event.fen,[...depthArgs,'--depth',String(Math.max(1,event.depth-event.reduction))])]});
      replay.push({name:'full-depth-forced-continuation-fresh-window',runs:[search(o.engine,event.fen,[...depthArgs,'--depth',String(event.depth)])]});
    }
    await save('subtree-replay.json',{event,variants:replay,evidence:'CONTROLLED_FRESH_STATE_SUBTREE_REPLAY',category:'UNKNOWN',reference_line_recovered:null,
      limitations:'Identical FEN and budget with repeated fresh processes. Original TT/history/stack/window are not restored. Inactive baseline heuristics are negative controls. Main SEE toggle does not disable qsearch SEE. No causal recovery claim without deeper external verification.'});
  }
}
