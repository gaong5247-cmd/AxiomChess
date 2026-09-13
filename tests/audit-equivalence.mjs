import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { Uci } from '../tools/stockfish-regression.mjs';
const before=resolve(process.argv[2]??'baseline/audit-rfp-stage.exe');
const after=resolve(process.argv[3]??'build/Release/axiom-0.4-audit.exe');
const output=process.argv[4]??'results/audit-equivalence.json';
const positions=(await readFile('tests/middlegame-balanced.fens','utf8')).split(/\r?\n/).filter(x=>x && !x.startsWith('#')).slice(0,100);
const engines=[new Uci(before),new Uci(after)];
let checked=0;
try {
  for(const e of engines) await e.init({Hash:8,Threads:1,ProofNodes:0,Middlegame:true});
  for(const fen of positions) {
    const results=[];
    for(const e of engines) results.push(await e.search(fen,{nodes:3000}));
    assert.equal(results[0].bestmove,results[1].bestmove,fen);
    // Search counters related to cache access may change; search results may not.
    assert(results[0].rows.length && results[1].rows.length,'Missing completed search iteration');
    for(const key of ['depth','nodes','type','score','pv']) assert.deepEqual(results[0].rows[0][key],results[1].rows[0][key],`${key}: ${fen}`);
    ++checked;
  }
} finally { for(const e of engines) e.close(); }
const hashes=await Promise.all([before,after].map(async path=>({path,sha256:createHash('sha256').update(await readFile(path)).digest('hex')})));
await writeFile(output,JSON.stringify({kind:'fixed_node_equivalence_not_elo',checked,nodes:3000,hashes,passed:true},null,2),{flag:'wx'});
console.log(`PASS ${checked} fixed-node before/after allocation optimization comparisons`);
