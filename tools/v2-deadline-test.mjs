import {spawn} from 'node:child_process';
import fs from 'node:fs';
const exe=process.argv[2]||'build-v2/Release/axiom-2.exe';
const p=spawn(exe);let partial='',waiting;const results=[];
p.stdout.on('data',b=>{partial+=b;let i;while((i=partial.indexOf('\n'))>=0){const s=partial.slice(0,i).trim();partial=partial.slice(i+1);if(waiting&&s.startsWith(waiting.prefix)){const w=waiting;waiting=null;clearTimeout(w.timer);w.resolve(s);}}});
function ask(command,prefix,timeout=3000){return new Promise((resolve,reject)=>{waiting={prefix,resolve,timer:setTimeout(()=>reject(Error('deadline timeout '+command)),timeout)};p.stdin.write(command+'\n');});}
try{
 await ask('uci','uciok');await ask('isready','readyok');
 for(let i=0;i<20;++i){p.stdin.write('ucinewgame\nposition startpos moves c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 d2d4 e5d4 f3d4 f8c5 d4c6\n');let start=process.hrtime.bigint();const best=await ask('go movetime 500','bestmove ');let ms=Number(process.hrtime.bigint()-start)/1e6;results.push({ms,best});if(ms>1000)throw Error('excessive deadline latency '+ms);}
 p.stdin.write('quit\n');console.log(JSON.stringify({passed:true,exe,results},null,2));fs.writeFileSync('results/axiom2/deadline-repro.json',JSON.stringify({passed:true,exe,results},null,2));
}catch(e){p.kill();console.error(e);process.exitCode=1;}
