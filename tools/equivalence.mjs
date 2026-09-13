// Fixed-node behavioral equivalence check for implementation-only optimizations.
import { Uci } from './stockfish-regression.mjs';
import { readFile,writeFile,access } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
const [oldPath,newPath,fenPath,output,nodesArg='5000']=process.argv.slice(2);
if(!output) throw new Error('Usage: node equivalence.mjs OLD.exe NEW.exe CORPUS.fens OUTPUT.json [nodes]');
try { await access(output); throw new Error('Output exists'); } catch(e) { if(e.code!=='ENOENT') throw e; }
const nodes=Number(nodesArg); if(!Number.isInteger(nodes) || nodes<1) throw new Error('Invalid nodes');
const a=new Uci(resolve(oldPath)),b=new Uci(resolve(newPath));
const rows=[];
try {
  for(const engine of [a,b]) await engine.init({Hash:32,Threads:1,ProofNodes:0,Middlegame:true});
  const fens=(await readFile(fenPath,'utf8')).split(/\r?\n/).filter(s=>s && !s.startsWith('#'));
  for(const fen of fens) {
    const old=await a.search(fen,{nodes}),fresh=await b.search(fen,{nodes});
    const signature=x=>JSON.stringify([x.bestmove,...x.rows.map(r=>[r.depth,r.type,r.score,r.nodes,r.pv])]);
    rows.push({fen,equal:signature(old)===signature(fresh),old,fresh});
    if(rows.length%100===0) console.log(`Compared ${rows.length}/${fens.length}; mismatches=${rows.filter(r=>!r.equal).length}`);
  }
  const hash=async p=>createHash('sha256').update(await readFile(p)).digest('hex');
  const report={kind:'fixed_node_behavioral_equivalence_not_strength',nodes,old_sha256:await hash(oldPath),new_sha256:await hash(newPath),corpus_sha256:await hash(fenPath),positions:rows.length,mismatches:rows.filter(r=>!r.equal).length,rows};
  await writeFile(output,JSON.stringify(report,null,2));
  console.log(JSON.stringify({positions:report.positions,mismatches:report.mismatches}));
  if(report.mismatches) process.exitCode=1;
} finally {a.close();b.close();}
