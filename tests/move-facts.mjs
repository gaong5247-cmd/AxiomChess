import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Uci} from '../tools/stockfish-regression.mjs';
import {sha} from '../tools/causal-analysis.mjs';
const exe=resolve(process.argv[2]??'build/Release/axiom-0.7-cost.exe'),out=process.argv[3]??'results/v07-move-facts-equivalence.json';
const feature=process.argv[4]??'reuse_move_facts';
const regular=(await readFile('tests/middlegame-balanced.fens','utf8')).split(/\r?\n/).filter(s=>s && !s.startsWith('#')).slice(0,100);
const critical=(await readFile('results/v06-critical-corpus/critical.jsonl','utf8')).trim().split(/\r?\n/).map(JSON.parse).slice(0,20).map(r=>r.fen);
const special=['r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1','1r5k/P7/8/8/8/8/8/7K w - - 0 1','4k3/8/8/r4pPK/8/8/8/8 w - f6 0 1'];
const engines=[new Uci(exe),new Uci(exe)];let n=0;
try {
  for(let i=0;i<2;++i) await engines[i].init({Middlegame:true,Threads:1,Hash:8,ProofNodes:0,[`Feature_${feature}`]:Boolean(i)});
  for(const fen of [...regular,...critical,...special]) {
    const budget=n<100?3000:20000,results=[];
    for(const e of engines) results.push(await e.search(fen,{nodes:budget}));
    assert.equal(results[0].bestmove,results[1].bestmove,fen);
    for(const key of ['depth','nodes','type','score','pv']) assert.deepEqual(results[0].rows[0]?.[key],results[1].rows[0]?.[key],`${key}: ${fen}`);
    ++n;
  }
} finally {for(const e of engines)e.close();}
await writeFile(out,JSON.stringify({n,passed:true,engine_sha256:await sha(exe),test_sha256:await sha(new URL(import.meta.url)),date:new Date().toISOString(),settings:`Middlegame, Threads=1, Hash=8, Proof=0; only ${feature} differs`},null,2),{flag:'wx'});
console.log(`PASS ${n} ${feature} OFF/ON exact fixed-node comparisons`);
