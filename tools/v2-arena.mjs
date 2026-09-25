import {spawn} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
const mode=process.argv[2]||'nodes';
const rounds=Number(process.argv[3]||8),budget=process.argv[4]||(mode==='nodes'?'10000':'10+0.1');
const directory=process.argv[5]||`results/axiom2/${mode}-${budget.replaceAll('+','_')}`;
fs.mkdirSync(directory,{recursive:true});
const exe=path.resolve('reference/fastchess-1.8.2/fastchess-windows-x86-64/fastchess.exe');
const v2=path.resolve('build-v2/Release/axiom-2.exe'),v1=path.resolve('build-v2/Release/axiom-1.0.exe');
const args=['-engine',`cmd=${v2}`,'name=Axiom2','option.Threads=1','-engine',`cmd=${mode==='ablation'||mode==='smp'?v2:v1}`,`name=${mode==='ablation'?'Axiom2-Decision':mode==='smp'?'Axiom2-4T':'Axiom1'}`,`option.Threads=${mode==='smp'?4:1}`];
if(mode==='ablation')args.push('option.DecisionImpact=true','option.RefutationSearch=true');
args.push('-each','proto=uci','option.Hash=32',mode==='nodes'?`nodes=${budget}`:`tc=${budget}`);
if(mode==='nodes')args.push('tc=600+5');
args.push('-openings',`file=${path.resolve('tests/axiom2-openings.epd')}`,'format=epd','order=sequential',`start=${process.argv[6]||1}`,
    '-rounds',String(rounds),'-repeat','-concurrency','2','-maxmoves','160',
    '-resign','movecount=5','score=900','twosided=true',
    '-draw','movenumber=60','movecount=10','score=10',
    '-pgnout',`file=${path.resolve(directory,'games.pgn')}`,'nodes=true','-output','format=cutechess','-ratinginterval','2');
fs.writeFileSync(path.join(directory,'command.json'),JSON.stringify({exe,args,notes:'Fixed manual opening corpus, paired colors, preliminary sample, not strength certification. No training or tuning.'},null,2));
const p=spawn(exe,args,{cwd:directory});let log='';
let lastTick=process.hrtime.bigint();const schedulingGaps=[];
const heartbeat=setInterval(()=>{const now=process.hrtime.bigint(),gap=Number(now-lastTick)/1e6;lastTick=now;if(gap>1000){const record={at:new Date().toISOString(),gap_ms:Math.round(gap)};schedulingGaps.push(record);console.error('runner scheduling gap',JSON.stringify(record));}},100);
p.stdout.on('data',b=>{log+=b;process.stdout.write(b);});p.stderr.on('data',b=>{log+=b;process.stderr.write(b);});
p.on('exit',code=>{clearInterval(heartbeat);fs.writeFileSync(path.join(directory,'scheduling-gaps.json'),JSON.stringify(schedulingGaps,null,2));fs.writeFileSync(path.join(directory,'arena.log'),log);process.exitCode=code??1;});
