import {spawnSync} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const converter=process.argv[2],out=process.argv[3];
if(!converter || !out) throw Error('Usage: node calibration-openings.mjs CONVERTER OUTPUT_DIRECTORY');
const lines=[
 'e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6', 'e4 e5 Nf3 Nc6 Bc4 Bc5 c3 Nf6',
 'e4 e5 Nf3 Nc6 d4 exd4 Nxd4 Nf6', 'e4 e5 Nf3 Nf6 Nxe5 d6 Nf3 Nxe4',
 'e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6', 'e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 g6',
 'e4 c5 Nf3 e6 d4 cxd4 Nxd4 a6', 'e4 c6 d4 d5 Nc3 dxe4 Nxe4 Bf5',
 'e4 e6 d4 d5 Nc3 Nf6 Bg5 Be7', 'e4 e6 d4 d5 e5 c5 c3 Nc6',
 'd4 d5 c4 e6 Nc3 Nf6 Nf3 Be7', 'd4 d5 c4 c6 Nf3 Nf6 Nc3 dxc4',
 'd4 Nf6 c4 g6 Nc3 Bg7 e4 d6', 'd4 Nf6 c4 e6 Nc3 Bb4 e3 O-O',
 'd4 Nf6 c4 e6 Nf3 b6 g3 Bb7', 'd4 Nf6 c4 c5 d5 e6 Nc3 exd5',
 'c4 e5 Nc3 Nf6 g3 d5 cxd5 Nxd5', 'c4 c5 Nf3 Nf6 Nc3 Nc6 g3 g6',
 'Nf3 d5 g3 Nf6 Bg2 e6 O-O Be7', 'd4 f5 g3 Nf6 Bg2 g6 Nf3 Bg7'
];
await mkdir(out);
const pgn=lines.map((line,i)=>`[Event "Calibration opening ${i+1}"]\n[Result "*"]\n\n${line} *\n`).join('\n');
const path=resolve(out,'source.pgn'); await writeFile(path,pgn);
const run=spawnSync(resolve(converter),[path],{encoding:'utf8',windowsHide:true});
if(run.status!==0) throw Error(run.stderr);
const finals=new Map(); for(const line of run.stdout.trim().split(/\r?\n/)) { const r=JSON.parse(line); finals.set(r.game,r.after_fen); }
if(finals.size!==20 || new Set(finals.values()).size!==20) throw Error('Expected 20 unique legal openings');
await writeFile(resolve(out,'openings.epd'),[...finals.values()].map(f=>f.split(' ').slice(0,4).join(' ')).join('\n')+'\n');
console.log('Verified 20 distinct opening positions through legal move replay');
