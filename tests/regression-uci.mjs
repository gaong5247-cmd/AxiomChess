import assert from 'node:assert/strict';
import { resolve,dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Uci } from '../tools/stockfish-regression.mjs';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const exe=resolve(root,'build/Release/axiom-0.3-cpu.exe');
const candidate=new Uci(exe);
try {
  await candidate.init({Hash:1,Threads:1,ProofNodes:0,Middlegame:true,Selective:true});
  assert(candidate.handshake.includes('option name Middlegame type check default false'));
  const result=await candidate.search('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',{nodes:2000});
  assert.match(result.bestmove,/^[a-h][1-8][a-h][1-8][qrbn]?$/);
  assert(result.rows.length && result.rows[0].depth>0 && result.rows[0].nodes<=2000);
  assert.equal(result.rows[0].pv[0],result.bestmove);
  const terms=await candidate.evaluation();
  assert('KingRingPressure' in terms && 'Outposts' in terms);
} finally { candidate.close(); }
const unsupported=new Uci(exe);
try { await assert.rejects(unsupported.init({DeliberatelyUnsupported:true}),/does not advertise/); }
finally { unsupported.close(); }
console.log('PASS regression UCI parser, required-option validation, score/PV and evaluation capture');
