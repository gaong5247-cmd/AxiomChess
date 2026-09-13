import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {Uci} from '../tools/stockfish-regression.mjs';
import {sha} from '../tools/causal-analysis.mjs';
const exe=resolve(process.argv[2]??'build/Release/axiom-1.0.exe'),out=process.argv[3];if(!out)throw Error('engine output-file required');
const u=new Uci(exe),reference=new Uci(resolve('baseline/v07-phase2.exe'));let tested=0;
try{
 await u.init({Threads:1,Hash:8,ProofNodes:0});
 assert(u.handshake.some(l=>l.includes('1.0-rc1')));
 assert(!u.handshake.some(l=>/^option name (Feature_|Experimental|Middlegame|SafetyMask|TimeGuard)/.test(l)));
 await reference.init({Threads:1,Hash:8,ProofNodes:0,Middlegame:true});
 const fens=(await readFile('tests/middlegame-balanced.fens','utf8')).split(/\r?\n/).filter(l=>l&&!l.startsWith('#')).slice(0,100);
 for(const fen of fens){const a=await u.search(fen,{nodes:3000}),b=await reference.search(fen,{nodes:3000});assert.equal(a.bestmove,b.bestmove,fen);for(const k of ['depth','nodes','type','score','pv'])assert.deepEqual(a.rows[0]?.[k],b.rows[0]?.[k],`${k}: ${fen}`);++tested;}
 for(const command of ['go nodes -1','go nodes 0','go depth nope','go movetime 1junk','go movetime','go ponder','go searchmoves e2e4','setoption name Feature_legal_fast_path value true']){
   u.send(command);await u.until(/^info string error /);u.send('isready');await u.until(/^readyok$/);
 }
 u.send('position startpos');u.send('go depth 3');const info=[];
 for(;;){let l=await u.read();if(l.startsWith('bestmove '))break;info.push(l);}
 const standard=info.find(l=>/^info depth 3 seldepth \d+ score (cp|mate) -?\d+ nodes \d+ time \d+ nps \d+ hashfull \d+ pv /.test(l));assert(standard,'Standard UCI info');
 assert(Number(standard.match(/hashfull (\d+)/)[1])<=1000);
 for(const command of ['go movetime 25','go wtime 100 btime 100 winc 0 binc 0','go wtime 2147483647 btime 2147483647 winc 2147483647 binc 2147483647 nodes 1000']){u.send(command);await u.until(/^bestmove /);}
 u.send('setoption name Threads value 3');u.send('go infinite');u.send('isready');await u.until(/^readyok$/);u.send('stop');await u.until(/^bestmove /);
}finally{u.close();reference.close();}
const bench=()=>{const r=spawnSync(exe,['--bench'],{encoding:'utf8',windowsHide:true});assert.equal(r.status,0,r.stderr);return JSON.parse(r.stdout);};
const a=bench(),b=bench();assert.equal(a.checksum,b.checksum);assert.equal(a.nodes,b.nodes);assert.equal(a.positions,5);
const blocked=spawnSync(exe,['--analyze','--feature','legal_fast_path'],{encoding:'utf8',windowsHide:true});assert.notEqual(blocked.status,0);assert.match(blocked.stderr,/research build/);
await writeFile(out,JSON.stringify({date:new Date().toISOString(),engine_sha256:await sha(exe),tested,passed:true,bench_runs:[a,b],description:'Production preset equivalence, clean options, strict go parsing, standard info, clocks, stop, bench determinism; no strength claim'},null,2),{flag:'wx'});
console.log(`PASS v1 UCI and ${tested} exact baseline searches; bench checksum ${a.checksum}`);
