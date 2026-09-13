import {spawnSync} from 'node:child_process';
import {readFile,writeFile,appendFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {Uci} from './stockfish-regression.mjs';
const o={pgn:'',converter:'',reference:'',out:'',threshold:100,depth:12,games:4,plies:60};
for(let i=2;i<process.argv.length;i+=2) { const key=process.argv[i].replace(/^--/,''); if(!(key in o) || !process.argv[i+1]) throw Error('Invalid option'); o[key]=typeof o[key]==='number'?Number(process.argv[i+1]):process.argv[i+1]; }
for(const key of ['pgn','converter','reference','out']) if(!o[key]) throw Error(`--${key} required; executable paths have no implicit default`);
for(const key of ['threshold','depth','games','plies']) if(!Number.isInteger(o[key]) || o[key]<1) throw Error(`Invalid ${key}`);
const parsed=spawnSync(resolve(o.converter),[resolve(o.pgn)],{encoding:'utf8',windowsHide:true,maxBuffer:32*1024*1024});
if(parsed.status!==0) throw Error(parsed.stderr||String(parsed.error));
const positions=parsed.stdout.trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
await mkdir(o.out);
const hash=async p=>createHash('sha256').update(await readFile(p)).digest('hex');
const manifest={options:o,pgn_sha256:await hash(o.pgn),reference_sha256:await hash(o.reference),source_positions:positions.length,created:new Date().toISOString(),policy:'First threshold crossing per game and side; null labels are unavailable original PGN telemetry. FEN replay omits repetition history; repeated positions excluded.'};
await writeFile(resolve(o.out,'manifest.json'),JSON.stringify(manifest,null,2));
const reference=new Uci(resolve(o.reference));
const previous=new Map(),done=new Set(); let examined=0,critical=0,mates=0,negative=0;
try {
  await reference.init({Hash:32,Threads:1});
  for(const p of positions) {
    if(p.game>o.games || p.ply>o.plies || p.repetitions>=2) continue;
    const key=`${p.game}:${p.side}`; if(done.has(key)) continue;
    const best=await reference.search(p.fen,{depth:o.depth,reference:true});
    const forced=await reference.search(p.fen,{depth:o.depth,reference:true,moves:[p.move]});
    const before=best.rows[0],after=forced.rows[0]; ++examined;
    if(!before || !after || before.type!=='cp' || after.type!=='cp') { ++mates; previous.delete(key); continue; }
    const rawLoss=before.score-after.score; if(rawLoss<0) ++negative;
    const loss=Math.max(0,rawLoss),prev=previous.get(key); previous.set(key,loss);
    const record={...p,axiom_move:p.move,axiom_score:null,axiom_depth:null,axiom_pv:null,reference_move:best.bestmove,reference_score_before:before.score,reference_score_after:after.score,reference_after_semantics:'root-side forced played move score at same depth, not child-side evaluation',cp_loss:loss,raw_cp_loss:rawLoss,category:p.phase==='endgame'?'endgame':'unclassified',previous_cp_loss:prev??null};
    await appendFile(resolve(o.out,'measured.jsonl'),JSON.stringify(record)+'\n');
    if(prev!=null && prev<o.threshold && loss>=o.threshold) {
      await appendFile(resolve(o.out,'critical.jsonl'),JSON.stringify(record)+'\n');
      done.add(key); ++critical;
    }
  }
} finally { reference.close(); }
await writeFile(resolve(o.out,'summary.json'),JSON.stringify({examined,critical,mate_cases_excluded:mates,negative_differences_clamped:negative,status:'bounded_reference_extraction_completed'},null,2));
console.log(JSON.stringify({examined,critical,mates}));
