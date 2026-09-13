import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Uci} from './stockfish-regression.mjs';
const [input,referencePath,output,limitText='20',depthText='12']=process.argv.slice(2);
if(!input || !referencePath || !output) throw Error('Usage: lmr-forensics.mjs SEARCH_JSON REFERENCE_EXE OUTPUT_JSON [limit] [depth]');
const limit=Number(limitText),depth=Number(depthText);
if(!Number.isInteger(limit) || limit<1 || !Number.isInteger(depth) || depth<1) throw Error('Invalid limit/depth');
const search=JSON.parse(await readFile(input,'utf8'));
const reference=new Uci(resolve(referencePath)); const findings=[];
try {
  await reference.init({Threads:1,Hash:32});
  const root=await reference.search(search.fen,{depth,reference:true});
  const samples=(search.lmr_samples??[]).filter(s=>s.reduction>=2 && !s.researched && s.root_move===root.bestmove && search.bestmove!==root.bestmove).slice(0,limit);
  for(const sample of samples) {
    const local=await reference.search(sample.fen,{depth,reference:true});
    findings.push({...sample,local_reference_move:local.bestmove,suspect:sample.move===local.bestmove});
  }
  await writeFile(output,JSON.stringify({kind:'reference_root_branch_reduction_suspects_not_proof',reference_depth:depth,root_reference_move:root.bestmove,axiom_move:search.bestmove,examined:findings.length,findings,truncated_sampling:true},null,2),{flag:'wx'});
} finally { reference.close(); }
