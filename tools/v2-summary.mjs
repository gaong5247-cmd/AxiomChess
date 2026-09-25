import fs from 'node:fs';
import path from 'node:path';
const dir='results/axiom2';
const summary={arenas:[],equal_nodes:[],benchmarks:[]};
for(const entry of fs.readdirSync(dir,{withFileTypes:true}))if(entry.isDirectory()){
    const logPath=path.join(dir,entry.name,'arena.log');if(!fs.existsSync(logPath))continue;const log=fs.readFileSync(logPath,'utf8');
    const scores=[...log.matchAll(/Score of (.+?) vs (.+?): (\d+) - (\d+) - (\d+)\s+\[([\d.]+)\] (\d+)/g)];if(!scores.length)continue;
    const s=scores.at(-1),elo=[...log.matchAll(/Elo difference: ([^,]+), LOS: ([\d.]+) %/g)].at(-1);
    const timeouts=(log.match(/loses on time/g)||[]).length;
    summary.arenas.push({directory:entry.name,engine:s[1],opponent:s[2],wins:+s[3],draws:+s[5],losses:+s[4],games:+s[7],score:(+s[3]+0.5*+s[5])/+s[7],elo:elo?.[1],los:elo?.[2],timeouts,complete:log.includes('Finished match'),sprt:'not run'});
}
if(fs.existsSync(path.join(dir,'equal-nodes.json'))){const rows=JSON.parse(fs.readFileSync(path.join(dir,'equal-nodes.json'),'utf8'));
    for(const r of rows){const tokens=r.info.split(' '),get=k=>tokens[tokens.indexOf(k)+1];summary.equal_nodes.push({engine:r.exe.includes('axiom-2')?'Axiom2':'Axiom1',fen:r.fen,budget:r.budget,nodes:+get('nodes'),depth:+get('depth'),ms:+get('time'),nps:+get('nps'),bestmove:r.bestmove});}}
for(const name of fs.readdirSync(dir))if(/^bench.*jsonl$/.test(name)){const rows=fs.readFileSync(path.join(dir,name),'utf8').trim().split(/\r?\n/).map(s=>JSON.parse(s));summary.benchmarks.push({file:name,...rows.at(-1)});}
fs.writeFileSync(path.join(dir,'summary.json'),JSON.stringify(summary,null,2));console.log(JSON.stringify(summary.arenas,null,2));
