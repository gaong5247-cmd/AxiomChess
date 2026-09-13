import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {sha} from './causal-analysis.mjs';
const read=async p=>JSON.parse(await readFile(p,'utf8'));
const off=await read('results/v07p2-profile-off/summary.json'),on=await read('results/v07p2-profile-on/summary.json');
const bench=await read('results/v07p2-benchmark/summary.json');
const data={
 baseline:{status:'FROZEN',A:'baseline/v06-final.exe',B:'baseline/v07-phase1.exe (reuse_move_facts ON)',C:'baseline/v07-phase2.exe (legal_fast_path ON, reuse_move_facts OFF)',note:'C2 isolated; no combined candidate. C OFF vs B binary OFF exact fixed-node test passed.'},
 legal:{status:'CANDIDATE_DEFAULT_OFF',old_pushes:off.sites.push.legal.calls,new_pushes:on.sites.push.legal.calls,eliminated:off.sites.push.legal.calls-on.sites.push.legal.calls,random_sample_positions:1000000,makes:997605,sequences:8499,unique_positions:'NOT_MEASURED',seed:2026091307,unmeasured:['piece-type/capture/quiet/pin per-category time and rejection matrix'],note:'Fast accept only non-check, non-king, unpinned, non-EP. Old oracle retained. Profile legal call count includes nested fallback scopes, not unique generations.'},
 undo:{status:'FULL_SNAPSHOT_RETAINED',incremental:'NOT_IMPLEMENTED',fuzz_evidence:'results/v07p2-legal-million.log',restored:['squares','side','castle','ep','clocks','FEN','hash','history','identities','proof_key']},
 attacks:{status:'MEASURED',off:off.counters.attacked,on:on.counters.attacked,off_sites:off.sites.attacked,on_sites:on.sites.attacked,node_local_pins:'Recomputed within each legal_moves call; no cross-mutation cache'},
 safety:{status:'DEFERRED_UNCHANGED',reason:'Type B safety policy must not contaminate isolated Type A trial; no new counterfactual model or weakening promoted.'},
 lmr:{status:'DEFERRED_UNCHANGED',reason:'Existing calibrated_lmr remains OFF; no new false-negative corpus or reduction calibration.'},
 history:{status:'DEFERRED_UNCHANGED',reason:'No decay/normalization changes; calibration curves unfinished.'},
 ordering:{status:'UNCHANGED',cutoff_index_distribution:'NOT_MEASURED',ordered_move_equivalence:'1M samples plus 123 search comparisons',reason:'No staged picker or ordering policy mixed into legal candidate.'},
 eval:{status:'UNCHANGED',static_vs_search_ratio:'NOT_MEASURED',term_ablation:'NOT_PERFORMED'},
 critical:{status:'PARTIAL',corpus:'results/v06-critical-corpus/critical.jsonl',reference:'Stockfish 18 depth18; 20-position benchmark only, not full 300-position time-quality run'},
 quiet:{status:'NOT_CREATED',reason:'General ordered-equivalence includes quiets, but does not substitute for independent high-confidence quiet-reference corpus.'},
 'fixed-depth':{status:'MEASURED',data:bench.filter(r=>r.mode==='depth')},
 'fixed-node':{status:'MEASURED',data:bench.filter(r=>r.mode==='nodes'),tests:['results/v07p2-search-equivalence.json','results/v07p2-off-equivalence.json']},
 'fixed-time':{status:'MIXED_QUALITY_NO_PROMOTION',data:bench.filter(r=>r.mode==='time'),quality:await read('results/v07p2-benchmark/fixed-time-quality.json')},
 sprt:{status:'SEE_RUNNER_RESULTS',evidence:['results/v07p2-paired/manifest.json','results/v07p2-paired/console.log','results/v07p2-phase1-replication/console.log'],note:'Each independent run reported separately; no pooled Elo or automatic default change'},
 rejected:{status:'NO_CORRECTNESS_REJECTION',legal_fast_path:'CANDIDATE OFF; mixed quality warrants holdout investigation',combined_candidate:'NOT_IMPLEMENTED',type_b:'NOT_IMPLEMENTED'}
};
const provenance={date:new Date().toISOString(),command:process.argv,tool_sha256:await sha(new URL(import.meta.url)),source_revision:'baseline/v07-phase2-source.zip',source_sha256:await sha('baseline/v07-phase2-source.zip'),binary_sha256:await sha('baseline/v07-phase2.exe'),compiler:'MSVC 19.51.36246 Release AVX2 IPO',benchmark_manifest:await read('results/v07p2-benchmark/manifest.json'),profile_manifest:await read('results/v07p2-profile-on/manifest.json')};
for(const [name,value] of Object.entries(data)) {await mkdir(`results/v07p2-${name}`);await writeFile(`results/v07p2-${name}/manifest.json`,JSON.stringify({...provenance,...value},null,2),{flag:'wx'});}
// Derive catastrophic-loss thresholds from the exact same fixed-time reference data.
const observations=await read('results/v07p2-benchmark/observations.json'),refs=await read('results/v07p2-benchmark/reference.json');
const tails=[false,true].map(enabled=>{
const rows=observations.filter(o=>o.mode==='time'&&o.enabled===enabled),losses=[];let mateRows=0;
for(const r of rows){const ref=refs[r.index],best=ref.ref.rows[0],played=ref.forced[r.result.bestmove]?.rows[0];if(best?.type==='cp'&&played?.type==='cp')losses.push(Math.max(0,best.score-played.score));else ++mateRows;}
return {enabled,observations:rows.length,unique_positions:20,cp_samples:losses.length,above_cp:Object.fromEntries([50,100,200,400].map(t=>[t,losses.filter(v=>v>t).length])),mate_valued_rows:mateRows,mate_miss:'NOT_CLASSIFIED',mate_allowed:'NOT_CLASSIFIED'};});
await writeFile('results/v07p2-fixed-time/tail-loss.json',JSON.stringify(tails,null,2),{flag:'wx'});
console.log('Created Phase2 evidence manifests; deferred work explicitly marked');
