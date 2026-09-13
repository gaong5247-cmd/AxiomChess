import {spawnSync} from 'node:child_process';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {sha} from './causal-analysis.mjs';
const exe=process.argv[2],out=process.argv[3];if(!exe||!out)throw Error('pgn converter output-directory');
// Different positions/lines, not merely a reshuffle of the Phase 1 opening set.
const lines=['e4 e5 Nc3 Nc6 f4 exf4 Nf3 g5','e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Nxe4',
'e4 e5 Nf3 Nc6 Bc4 Nf6 d3 Bc5','e4 e5 Nf3 d6 d4 Nf6 Nc3 Nbd7',
'e4 c5 Nc3 Nc6 g3 g6 Bg2 Bg7','e4 c5 Nf3 d6 Bb5+ Bd7 Bxd7+ Qxd7',
'e4 c5 c3 Nf6 e5 Nd5 d4 cxd4','e4 e6 d4 d5 Nd2 c5 exd5 exd5',
'e4 c6 d4 d5 e5 Bf5 Nf3 e6','e4 d5 exd5 Qxd5 Nc3 Qa5 d4 Nf6',
'e4 d6 d4 Nf6 Nc3 g6 f4 Bg7','d4 d5 Nf3 Nf6 Bf4 e6 e3 c5',
'd4 d5 c4 dxc4 Nf3 Nf6 e3 e6','d4 Nf6 c4 g6 g3 Bg7 Bg2 d5',
'd4 Nf6 c4 c5 d5 b5 cxb5 a6','d4 Nf6 c4 e5 dxe5 Ng4 Nf3 Nc6',
'c4 e6 Nc3 d5 d4 Nf6 Bg5 Be7','Nf3 Nf6 c4 b6 g3 Bb7 Bg2 e6',
'b3 e5 Bb2 Nc6 e3 Nf6 Bb5 Bd6','f4 d5 Nf3 Nf6 e3 g6 b3 Bg7'];
await mkdir(out);const pgn=lines.map((l,i)=>`[Event "Phase2 replication ${i}"]\n[Result "*"]\n\n${l} *\n`).join('\n');
await writeFile(`${out}/source.pgn`,pgn,{flag:'wx'});
const run=spawnSync(resolve(exe),[resolve(out,'source.pgn')],{encoding:'utf8',windowsHide:true});if(run.status!==0)throw Error(run.stderr);
const map=new Map();for(const line of run.stdout.trim().split(/\r?\n/)){const r=JSON.parse(line);map.set(r.game,r.after_fen.split(' ').slice(0,4).join(' '));}
const old=new Set((await readFile('results/v05-openings/openings.epd','utf8')).trim().split(/\r?\n/));
if(map.size!==20||new Set(map.values()).size!==20||[...map.values()].some(f=>old.has(f)))throw Error('Opening overlap/invalid corpus');
await writeFile(`${out}/openings.epd`,[...map.values()].reverse().join('\n')+'\n',{flag:'wx'});
await writeFile(`${out}/manifest.json`,JSON.stringify({date:new Date().toISOString(),seed:2026091302,positions:20,overlap_with_phase1:0,converter_sha256:await sha(exe),openings_sha256:await sha(`${out}/openings.epd`),method:'Independent manually selected opening lines, reverse ordering, verified legal PGN replay. Related opening families can remain.'},null,2),{flag:'wx'});
console.log('Validated 20 new, disjoint opening positions');
