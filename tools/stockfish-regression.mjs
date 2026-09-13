import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { readFile, writeFile, appendFile, mkdir, access } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const defaults={engine:resolve(root,'build/Release/axiom-0.3-cpu.exe'),stockfish:resolve(root,'reference/stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe'),
  baseline:'',fens:resolve(root,'tests/middlegame-balanced.fens'),out:resolve(root,'results/regression'),depth:18,nodes:5000,limit:1000,target:500,diagnose:0,threads:1};
const options={...defaults};
const direct=process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url);
if(direct) for(let i=2;i<process.argv.length;i++) {
  const key=process.argv[i].replace(/^--/,'');
  if(!(key in options) || i+1>=process.argv.length) throw new Error(`Unknown/missing option ${process.argv[i]}`);
  options[key]=typeof defaults[key]==='number'?Number(process.argv[++i]):resolve(process.argv[++i]);
}
for(const key of ['depth','nodes','limit','target','diagnose','threads']) if(!Number.isInteger(options[key]) || options[key]<0) throw new Error(`Invalid ${key}`);
if(options.depth<1 || options.nodes<1 || options.limit<1) throw new Error('Positive depth, nodes, limit required');
if(options.threads<1 || options.threads>16) throw new Error('Reference threads must be 1..16');

export class Uci {
  constructor(exe) {
    this.process=spawn(exe,[],{windowsHide:true,stdio:['pipe','pipe','pipe']});
    this.lines=[]; this.pending=null; this.error=null; this.stderr='';
    this.process.stderr.on('data',data=>{this.stderr=(this.stderr+data).slice(-4000);});
    createInterface({input:this.process.stdout}).on('line',line=>{
      if(this.pending) { const wait=this.pending; this.pending=null; clearTimeout(wait.timer); wait.resolve(line); }
      else this.lines.push(line);
    });
    this.process.on('error',error=>this.fail(error));
    this.process.on('exit',code=>this.fail(new Error(`Engine exited (${code}): ${this.stderr}`)));
  }
  fail(error) { this.error=error; if(this.pending) { clearTimeout(this.pending.timer); this.pending.reject(error); this.pending=null; } }
  send(line) { if(this.error) throw this.error; this.process.stdin.write(line+'\n'); }
  read() {
    if(this.lines.length) return Promise.resolve(this.lines.shift());
    if(this.error) return Promise.reject(this.error);
    return new Promise((resolveLine,reject)=>{
      this.pending={resolve:resolveLine,reject,timer:setTimeout(()=>{this.pending=null; reject(new Error('UCI response timeout'));},60000)};
    });
  }
  async until(pattern) { for(;;) { const line=await this.read(); if(pattern.test(line)) return line; } }
  async init(settings={}) {
    this.send('uci'); const handshake=[];
    for(;;) { const line=await this.read(); handshake.push(line); if(line==='uciok') break; }
    const available=new Set(handshake.flatMap(line=>{const match=line.match(/^option name (.+) type /); return match?[match[1]]:[];}));
    for(const [name,value] of Object.entries(settings)) {
      if(!available.has(name)) throw new Error(`Engine does not advertise required UCI option: ${name}`);
      this.send(`setoption name ${name} value ${value}`);
    }
    this.handshake=handshake; this.settings=settings;
    this.send('isready'); await this.until(/^readyok$/);
  }
  async search(fen,{depth=0,nodes=0,multipv=1,moves=[],reference=false}={}) {
    this.send('ucinewgame');
    if(reference) this.send(`setoption name MultiPV value ${multipv}`);
    this.send('isready'); await this.until(/^readyok$/);
    this.send(`position fen ${fen}`);
    const command=`go${depth?' depth '+depth:''}${nodes?' nodes '+nodes:''}${moves.length?' searchmoves '+moves.join(' '):''}`;
    const rows=new Map(),iterations=[]; const start=performance.now(); this.send(command);
    for(;;) {
      const line=await this.read();
      if(line.startsWith('info string error')) throw new Error(line);
      if(line.startsWith('bestmove ')) return {bestmove:line.split(/\s+/)[1],rows:[...rows.values()].sort((a,b)=>a.multipv-b.multipv),iterations,wall_ms:performance.now()-start};
      const d=line.match(/\bdepth (\d+)/),score=line.match(/\bscore (cp|mate) (-?\d+)/),pv=line.match(/\bpv (.+)$/);
      if(!d || !score || !pv || /\b(?:lowerbound|upperbound)\b/.test(line)) continue;
      const slot=Number(line.match(/\bmultipv (\d+)/)?.[1]??1),n=Number(line.match(/\bnodes (\d+)/)?.[1]??0);
      const row={depth:Number(d[1]),multipv:slot,type:score[1],score:Number(score[2]),pv:pv[1].split(/\s+/),nodes:n};
      iterations.push(row);
      if(!rows.has(slot) || rows.get(slot).depth<=row.depth) rows.set(slot,row);
    }
  }
  async evaluation() { this.send('eval'); const line=await this.until(/^info string eval /); return Object.fromEntries([...line.matchAll(/(\w+)=(-?\d+)/g)].map(m=>[m[1],Number(m[2])])); }
  close() { if(!this.error) this.process.stdin.write('quit\n'); this.process.stdin.end(); const timer=setTimeout(()=>this.process.kill(),2000); this.process.once('exit',()=>clearTimeout(timer)); }
}
export function summarize(rows) {
  const valid=rows.filter(r=>r.status==='measured');
  const stats=side=>{
    const entries=valid.map(r=>r[side]).filter(Boolean),losses=entries.map(r=>r.cp_loss).filter(v=>typeof v==='number').sort((a,b)=>a-b);
    const n=entries.length,m=losses.length;
    return {positions:n,cp_positions:m,mate_positions:n-m,
      top1_agreement:n?entries.filter(r=>r.top1).length/n:null,top3_agreement:n?entries.filter(r=>r.top3).length/n:null,
      mean_cp_loss:m?losses.reduce((a,b)=>a+b,0)/m:null,median_cp_loss:m?(losses[Math.floor((m-1)/2)]+losses[Math.floor(m/2)])/2:null,
      p95_cp_loss:m?losses[Math.ceil(m*.95)-1]:null,max_cp_loss:m?losses[m-1]:null,
      blunder_rate:m?losses.filter(v=>v>200).length/m:null,major_error_rate:m?losses.filter(v=>v>100).length/m:null,
      mate_regressions:entries.filter(r=>r.mate_regression).length};
  };
  return {attempted:rows.length,measured:valid.length,excluded:rows.length-valid.length,candidate:stats('candidate'),baseline:stats('baseline')};
}
function measure(result,ref,score) {
  const best=ref.rows[0];
  const cp=best.type==='cp' && score.type==='cp';
  return {move:result.bestmove,depth:result.rows[0]?.depth??0,nodes:result.rows[0]?.nodes??0,wall_ms:result.wall_ms,
    top1:result.bestmove===ref.bestmove,top3:ref.rows.some(r=>r.pv[0]===result.bestmove),
    cp_loss:cp?Math.max(0,best.score-score.score):null,raw_cp_loss:cp?best.score-score.score:null,
    mate_regression:(best.type==='mate' && best.score>0 && !(score.type==='mate' && score.score>0)) || (score.type==='mate' && score.score<0 && !(best.type==='mate' && best.score<0)),
    reference_score:score};
}
function classify(fen,entry,terms) {
  const tags=[];
  if((fen.split(' ')[0].match(/[a-z]/gi)??[]).length<=10) tags.push('endgame');
  if(entry.mate_regression) tags.push('tactical');
  if(Math.abs(terms.KingRingPressure??0)+Math.abs(terms.KingOpenFiles??0)+Math.abs(terms.KingCoordination??0)>50) tags.push('king_safety');
  if(Math.abs(terms.PawnStructure??0)+Math.abs(terms.PassedPawns??0)>70) tags.push('pawn_structure');
  if(!tags.length) tags.push('positional');
  return tags;
}
async function main() {
  try { await access(options.out); throw new Error('Output directory exists; choose a new --out'); } catch(e) { if(e.code!=='ENOENT') throw e; }
  await mkdir(options.out,{recursive:true});
  const fens=(await readFile(options.fens,'utf8')).split(/\r?\n/).map(s=>s.trim()).filter(s=>s && !s.startsWith('#')).slice(0,options.limit);
  const sha=async path=>createHash('sha256').update(await readFile(path)).digest('hex');
  const manifest={...options,started:new Date().toISOString(),engine_sha256:await sha(options.engine),stockfish_sha256:await sha(options.stockfish),baseline_sha256:options.baseline?await sha(options.baseline):null,
    corpus_sha256:await sha(options.fens),
    interpretation:'Fixed-node diagnostic against fixed-depth Stockfish. CP loss is reference-estimated, not a proof or Elo. Mate scores excluded from CP summaries. Error labels are hypotheses.'};
  await writeFile(resolve(options.out,'manifest.json'),JSON.stringify(manifest,null,2));
  const candidate=new Uci(options.engine),stockfish=new Uci(options.stockfish),baseline=options.baseline?new Uci(options.baseline):null;
  const rows=[],failures=[]; let diagnosed=0,stopped=false;
  try {
    await candidate.init({Hash:32,Threads:1,ProofNodes:0,Middlegame:true});
    await stockfish.init({Hash:64,Threads:options.threads,MultiPV:3});
    if(baseline) await baseline.init({Hash:32,Threads:1,ProofNodes:0,Feature_correction:true,Feature_continuation:true,Feature_capture_history:true,Feature_countermove:true,Feature_dynamic_lmr:true});
    await writeFile(resolve(options.out,'uci-settings.json'),JSON.stringify({candidate:{handshake:candidate.handshake,settings:candidate.settings},reference:{handshake:stockfish.handshake,settings:stockfish.settings},baseline:baseline?{handshake:baseline.handshake,settings:baseline.settings}:null},null,2));
    for(let index=0;index<fens.length;index++) {
      try { await access(resolve(options.out,'STOP')); stopped=true; break; } catch(e) { if(e.code!=='ENOENT') throw e; }
      const fen=fens[index]; const result=await candidate.search(fen,{nodes:options.nodes});
      const old=baseline?await baseline.search(fen,{nodes:options.nodes}):null;
      const ref=await stockfish.search(fen,{depth:options.depth,multipv:3,reference:true});
      const record={index,fen,status:'measured',reference:ref};
      if(!ref.rows.length || ref.rows.some(r=>r.depth<options.depth)) record.status='reference_incomplete';
      else {
        const moveScores=new Map(ref.rows.map(r=>[r.pv[0],r]));
        const scoreMove=async move=>{
          const cached=moveScores.get(move); if(cached) return cached;
          const forced=await stockfish.search(fen,{depth:options.depth,moves:[move],reference:true});
          if(!forced.rows.length || forced.rows[0].depth<options.depth) throw new Error('Forced reference search incomplete');
          moveScores.set(move,forced.rows[0]); return forced.rows[0];
        };
        record.candidate=measure(result,ref,await scoreMove(result.bestmove));
        if(old) record.baseline=measure(old,ref,await scoreMove(old.bestmove));
        record.evaluation=await candidate.evaluation();
        record.error_tags=classify(fen,record.candidate,record.evaluation);
        record.classification_confidence='heuristic_hypothesis';
        if(diagnosed<options.diagnose && record.candidate.cp_loss>100 && record.candidate.depth>0) {
          diagnosed++;
          candidate.send('setoption name Selective value false');
          const safe=await candidate.search(fen,{depth:record.candidate.depth,nodes:options.nodes*8});
          candidate.send('setoption name Selective value true');
          record.nonselective={...safe,complete:(safe.rows[0]?.depth??0)>=record.candidate.depth};
          if(record.nonselective.complete && safe.bestmove===ref.bestmove) record.error_tags.push('LMR/pruning_candidate');
          const deeper=await candidate.search(fen,{depth:record.candidate.depth+2,nodes:options.nodes*8});
          record.deeper={...deeper,complete:(deeper.rows[0]?.depth??0)>=record.candidate.depth+2};
          if(record.deeper.complete && deeper.bestmove===ref.bestmove) record.error_tags.push('horizon_candidate');
        }
        if(record.candidate.cp_loss>0 || record.candidate.mate_regression) failures.push(fen);
      }
      rows.push(record); await appendFile(resolve(options.out,'positions.jsonl'),JSON.stringify(record)+'\n');
      await writeFile(resolve(options.out,'summary.json'),JSON.stringify(summarize(rows),null,2));
      await writeFile(resolve(options.out,'reference-errors.fens'),failures.join('\n')+'\n');
      if((index+1)%10===0) console.log(JSON.stringify({processed:index+1,errors:failures.length,...summarize(rows).candidate}));
      if(options.target && failures.length>=options.target) break;
    }
    await writeFile(resolve(options.out,'completion.json'),JSON.stringify({finished:new Date().toISOString(),stopped,positions:rows.length,reference_errors:failures.length,target_reached:options.target>0 && failures.length>=options.target},null,2));
    console.log(JSON.stringify(summarize(rows),null,2));
  } finally { candidate.close(); stockfish.close(); baseline?.close(); }
}
if(direct) await main();
