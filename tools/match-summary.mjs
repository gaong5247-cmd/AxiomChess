import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const directory=process.argv[2]; if(!directory) throw Error('Match directory required');
const text=await readFile(resolve(directory,'games.pgn'),'utf8');
const games=text.split(/(?=\[Event ")/).filter(x=>x.startsWith('[Event'));
const players={},terminations={};
for(const game of games) {
  const tags=Object.fromEntries([...game.matchAll(/^\[(\w+) "([^"]*)"\]/gm)].map(m=>[m[1],m[2]]));
  const names=[tags.White,tags.Black];
  for(const name of names) players[name]??={wins:0,losses:0,draws:0,time_forfeits:0,illegal_moves:0};
  terminations[tags.Termination]=(terminations[tags.Termination]??0)+1;
  if(tags.Result==='1/2-1/2') for(const name of names) ++players[name].draws;
  else if(['1-0','0-1'].includes(tags.Result)) {
    const winner=tags.Result==='1-0'?0:1,loser=1-winner;
    ++players[names[winner]].wins; ++players[names[loser]].losses;
    if(tags.Termination==='time forfeit') ++players[names[loser]].time_forfeits;
    if(tags.Termination==='illegal move') ++players[names[loser]].illegal_moves;
  }
}
const summary={games:games.length,players,terminations};
await writeFile(resolve(directory,'game-summary.json'),JSON.stringify(summary,null,2),{flag:'wx'});
console.log(JSON.stringify(summary));
