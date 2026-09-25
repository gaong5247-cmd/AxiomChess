import {spawn} from 'node:child_process';
import {performance} from 'node:perf_hooks';
import fs from 'node:fs';
const exe=process.argv[2] || 'build-v2/Release/axiom-2.exe';
const p=spawn(exe,[],{stdio:['pipe','pipe','pipe']});
let text='',errors='',lines=[],pending=[];
p.stdout.on('data',b=>{text+=b;let i;while((i=text.indexOf('\n'))>=0){const line=text.slice(0,i).trim();text=text.slice(i+1);lines.push(line);for(const w of [...pending])if(w.match(line)){pending=pending.filter(x=>x!==w);clearTimeout(w.timer);w.resolve(line);}}});
p.stderr.on('data',b=>errors+=b);
const send=s=>p.stdin.write(s+'\n');
function wait(match,ms=30000){return new Promise((resolve,reject)=>{const w={match,resolve,timer:setTimeout(()=>{pending=pending.filter(x=>x!==w);reject(Error('timeout; last lines: '+lines.slice(-5).join('\n')));},ms)};pending.push(w);});}
function assert(ok,why){if(!ok)throw Error(why);}
async function command(s,prefix,ms){const result=wait(l=>l.startsWith(prefix),ms);send(s);return result;}
const results=[];
async function search(commandText,setup=[]){for(const line of setup)send(line);const start=performance.now(),begin=lines.length;const best=await command(commandText,'bestmove ');const info=lines.slice(begin).filter(l=>l.startsWith('info depth'));
    assert(!lines.slice(begin).some(l=>l.includes(' error ')),'engine reported error');results.push({command:commandText,best,elapsed_ms:Math.round(performance.now()-start),last_info:info.at(-1)});return {best,info};}
try{
    await command('uci','uciok');await command('isready','readyok');
    const options=lines.join('\n');for(const name of ['Hash','Threads','MultiPV','Ponder','Move Overhead','SyzygyPath'])assert(options.includes('option name '+name+' '),'missing option '+name);
    await search('go depth 10',['position startpos']);
    await search('go movetime 1000',['ucinewgame','position startpos']);
    assert(results.at(-1).elapsed_ms<1600,'movetime exceeded tolerance');
    const nodes=await search('go nodes 100000',['ucinewgame','position startpos']);assert(!nodes.best.includes('0000'),'node search no move');
    const restricted=await search('go searchmoves e2e4 d2d4 depth 5',['ucinewgame','position startpos']);assert(/bestmove (e2e4|d2d4)/.test(restricted.best),'searchmoves escaped');
    const repetition=await search('go searchmoves f6g8 depth 4',['ucinewgame','position startpos moves g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1']);
    assert(repetition.info.every(s=>s.split(' pv ')[1]==='f6g8'),'PV continued beyond a draw claim');
    const multi=await search('go depth 4',['setoption name MultiPV value 3','position startpos']);for(const n of [1,2,3])assert(multi.info.some(s=>s.includes('multipv '+n+' ')),'missing MultiPV '+n);
    send('setoption name MultiPV value 1');send('position startpos');send('go infinite');await new Promise(r=>setTimeout(r,100));const start=performance.now();await command('stop','bestmove ',2000);results.push({command:'go infinite / stop',elapsed_ms:Math.round(performance.now()-start)});
    send('position startpos');let before=lines.filter(l=>l.startsWith('bestmove')).length;send('go ponder depth 3');await new Promise(r=>setTimeout(r,200));assert(lines.filter(l=>l.startsWith('bestmove')).length===before,'premature ponder bestmove');await command('ponderhit','bestmove ',3000);
    send('position startpos');send('go ponder wtime 3000 btime 3000');await new Promise(r=>setTimeout(r,100));await command('ponderhit','bestmove ',3000);
    for(let i=0;i<8;++i)await search('go nodes 10000',['setoption name Threads value 4','ucinewgame','position startpos']);
    send('setoption name Threads value 1');send('position fen this is invalid');await command('isready','readyok');await search('go depth 3',['position startpos']);
    await search('go movetime 1',['position startpos']);
    await search('go wtime 100 btime 100 movestogo 1',['position startpos']);
    await search('go wtime -10 btime -10',['position startpos']);
    const terminal=await search('go depth 4',['position fen 7k/6Q1/6K1/8/8/8/8/8 b - - 150 1']);assert(terminal.best==='bestmove 0000','mate no move');
    send('quit');await new Promise(resolve=>p.on('exit',resolve));
    assert(!errors,'stderr: '+errors);const report={passed:true,exe,results};console.log(JSON.stringify(report,null,2));
    if(process.argv[3])fs.writeFileSync(process.argv[3],JSON.stringify(report,null,2));
}catch(e){p.kill();console.error(e.stack);process.exitCode=1;}
