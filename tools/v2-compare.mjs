import {spawn} from 'node:child_process';
import fs from 'node:fs';
const fens=fs.readFileSync('tests/bench.fens','utf8').split(/\r?\n/).filter(s=>s&&!s.startsWith('#'));
const budgets=(process.argv[2]||'10000,50000,100000,500000,1000000').split(',').map(Number);
const report=[];
async function run(exe,fen,nodes){return new Promise((resolve,reject)=>{
    const p=spawn(exe);let input='',last='',timer=setTimeout(()=>{p.kill();reject(Error('timeout'));},180000);const start=Date.now();
    p.stdout.on('data',chunk=>{input+=chunk;let i;while((i=input.indexOf('\n'))>=0){const s=input.slice(0,i).trim();input=input.slice(i+1);if(s.startsWith('info depth'))last=s;if(s==='uciok')p.stdin.write('isready\n');if(s==='readyok')p.stdin.write('position fen '+fen+'\ngo nodes '+nodes+'\n');if(s.startsWith('bestmove')){clearTimeout(timer);p.stdin.write('quit\n');resolve({exe,fen,budget:nodes,bestmove:s.split(' ')[1],info:last,wall_ms:Date.now()-start});}}});
    p.on('error',reject);p.stdin.write('uci\n');
});}
for(const nodes of budgets)for(let i=0;i<fens.length;++i)for(const exe of ['build-v2/Release/axiom-1.0.exe','build-v2/Release/axiom-2.exe']){
    const r=await run(exe,fens[i],nodes);report.push(r);console.log(JSON.stringify(r));
    fs.mkdirSync('results/axiom2',{recursive:true});fs.writeFileSync('results/axiom2/equal-nodes.json',JSON.stringify(report,null,2));
}
