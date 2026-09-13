import {Uci} from '../tools/stockfish-regression.mjs';
import {readFile,writeFile} from 'node:fs/promises';
import {sha} from '../tools/causal-analysis.mjs';
const exe=process.argv[2],out=process.argv[3];
if(!exe || !out) throw Error('engine output-file required');
const fens=(await readFile('tests/bench.fens','utf8')).split(/\r?\n/).filter(s=>s && !s.startsWith('#'));
const results=[];
for(const enabled of [false,true]) {
  const uci=new Uci(exe);
  try {
    await uci.init({Threads:1,Hash:32,ProofNodes:0,Middlegame:true,Feature_reuse_move_facts:enabled});
    for(const fen of fens) {
      uci.send('ucinewgame');uci.send('isready');await uci.until(/^readyok$/);
      uci.send(`position fen ${fen}`);uci.send('go infinite');
      await new Promise(r=>setTimeout(r,100));
      const start=performance.now();uci.send('stop');
      const move=await uci.until(/^bestmove /),latency=performance.now()-start;
      results.push({fen,enabled,stop_to_bestmove_ms:latency,move,subsystem:'UNKNOWN'});
      if(latency>1000) throw Error(`Stop response exceeded 1 s: ${latency}`);
    }
  } finally {uci.close();}
}
await writeFile(out,JSON.stringify({engine_sha256:await sha(exe),date:new Date().toISOString(),command:process.argv,interpretation:'External UCI latency includes IPC and scheduling; subsystem at request unobserved. Ten samples do not establish a worst-case bound.',results},null,2),{flag:'wx'});
console.log(`PASS ${results.length} stop probes; max ${Math.max(...results.map(r=>r.stop_to_bestmove_ms)).toFixed(2)} ms`);
