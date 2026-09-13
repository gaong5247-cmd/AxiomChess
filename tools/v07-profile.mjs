import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {search,sha} from './causal-analysis.mjs';
const exe=process.argv[2],out=process.argv[3],extra=process.argv.slice(4);
if(!exe || !out) throw Error('engine output-directory [extra engine flags] required');
await mkdir(out);
const corpus=(await readFile('tests/bench.fens','utf8')).split(/\r?\n/).filter(s=>s && !s.startsWith('#'));
const observations=corpus.map(fen=>search(exe,fen,['--depth','100','--nodes','20000','--cost-profile',...extra]));
const counters={},sites={},legal_classes={};let bytes=0,nodes=0;
for(const r of observations) {
  if(!r.hot_profile) throw Error('Cost instrumentation absent');
  nodes+=r.nodes;bytes+=r.hot_profile.board_snapshot_bytes_copied;
  for(const [name,c] of Object.entries(r.hot_profile.legal_filter_classes??{})) {const dest=legal_classes[name]??={generated:0,legal:0,illegal:0,pushes:0,attacked:0,elapsed_ns:0};for(const k in c)dest[k]+=c[k];}
  for(const [name,c] of Object.entries(r.hot_profile.counters)) {const dest=counters[name]??={calls:0,inclusive_ns:0,exclusive_ns:0};for(const k in c) dest[k]+=c[k];}
  for(const [name,domains] of Object.entries(r.hot_profile.call_sites)) for(const [domain,c] of Object.entries(domains)) {const dest=(sites[name]??={})[domain]??={calls:0,inclusive_ns:0,exclusive_ns:0};for(const k in c) dest[k]+=c[k];}
}
const save=(name,data)=>writeFile(resolve(out,name),JSON.stringify(data,null,2),{flag:'wx'});
await save('observations.json',observations);
await save('manifest.json',{engine_sha256:await sha(exe),corpus_sha256:await sha('tests/bench.fens'),tool_sha256:await sha(new URL(import.meta.url)),command:process.argv,compiler:'MSVC 19.51 Release AVX2 IPO research build',date:new Date().toISOString(),nodes_per_position:20000,threads:1,hash_mb:32,features:'Middlegame; trace disabled',limits:'depth 100, no time limit, 20000 nodes',interpretation:'Instrumented timings include observer overhead; exclusive time subtracts nested instrumented scopes. Not uninstrumented CPU performance. Logical bytes count snapshot payloads, not measured memory traffic.'});
await save('summary.json',{positions:observations.length,nodes,logical_board_snapshot_bytes:bytes,pushes_per_node:counters.push.calls/nodes,counters,sites,legal_classes,top10_exclusive:Object.entries(counters).sort((a,b)=>b[1].exclusive_ns-a[1].exclusive_ns).slice(0,10)});
console.log(`Profiled ${observations.length} positions, ${counters.push.calls} pushes, ${counters.see.calls} SEE calls`);
