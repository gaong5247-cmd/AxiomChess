import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {Uci} from '../tools/stockfish-regression.mjs';
const exe=resolve(process.argv[2]??'build/Release/axiom-0.6-attribution.exe');
const run=args=>{const p=spawnSync(exe,['--analyze','--depth','4','--time','0','--nodes','20000','--proof-nodes','0','--middlegame',...args],{encoding:'utf8',windowsHide:true}); assert.equal(p.status,0,p.stderr); return JSON.parse(p.stdout);};
for(const extra of [[],['--partial-ordering'],['--safety-mask','none'],['--safety-mask','all'],['--no-lmr','--no-null'],['--force-root-move','a2a3']]) {
  const r=run(extra);
  assert.equal(r.qply_histogram.reduce((a,b)=>a+b,0),r.search_stats.qnodes);
  assert.equal(new Set(r.moves.map(m=>m.move)).size,r.moves.length);
  assert(r.qsearch_ratio>=0 && r.qsearch_ratio<=1);
  assert(r.moves.every(m=>m.principal_variation[0]===m.move));
  if(extra.includes('--no-lmr')) assert.equal(r.search_stats.lmr_reductions,0);
  if(extra.includes('--no-null')) assert.equal(r.search_stats.null_attempts,0);
  if(extra.includes('--force-root-move')) {assert.equal(r.bestmove,'a2a3'); assert.equal(r.moves.length,1);}
  if(extra.includes('none')) assert(Object.values(r.safety_skipped_checks).every(row=>Object.values(row).every(v=>v===0)));
}
const uci=new Uci(exe);
try {
  await uci.init({SafetyMask:'no-king',TimeGuard:false,ProofNodes:0});
  uci.send('setoption name SafetyMask value 16384');
  assert.match(await uci.until(/^info string error /),/Safety mask/);
  uci.send('setoption name SafetyMask value legacy');
  uci.send('isready'); await uci.until(/^readyok$/);
} finally {uci.close();}
console.log('PASS v06 JSON accounting, masks, forced replay, partial ordering uniqueness, UCI validation');
