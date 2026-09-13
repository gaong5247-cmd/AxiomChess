// Derived evidence index. Never converts an unmeasured item into a zero-cost claim.
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {sha} from './causal-analysis.mjs';
const read=async p=>JSON.parse(await readFile(p,'utf8'));
const off=await read('results/v07-profile-off/summary.json'),on=await read('results/v07-profile-on/summary.json');
const obs=await read('results/v07-profile-off/observations.json');
const sum=k=>obs.reduce((s,r)=>s+(r.search_stats[k]??0),0);
const benchmark=await read('results/v07-candidate-a/summary.json');
const groups={
 baseline:{status:'FROZEN',evidence:['baseline/v06-final.exe','baseline/v06-final-source.zip']},
 profile:{status:'PARTIAL',evidence:['results/v07-profile-off/summary.json','results/v07-profile-on/summary.json'],top10_exclusive:off.top10_exclusive,unmeasured:['strategic_eval','phase','pawn_terms','individual material/PST/mobility terms']},
 board:{status:'MEASURED_NO_UNDO_REWRITE',calls:{off:off.counters.push.calls,on:on.counters.push.calls},logical_snapshot_payload_bytes_per_push:512,off_logical_bytes:off.logical_board_snapshot_bytes,on_logical_bytes:on.logical_board_snapshot_bytes,origins:off.sites.push,not_measured:'Actual cache/memory traffic; incremental Undo equivalence not applicable'},
 legalgen:{status:'PARTIAL_NO_GENERATOR_REWRITE',counters:{legal:off.counters.legal,pseudo:off.counters.pseudo,attacked:off.counters.attacked},attack_origins:off.sites.attacked,unmeasured:['pin/check-evasion/special-move separate timers','pseudo rejection count']},
 qsearch:{status:'PARTIAL_UNCHANGED_POLICY',qnodes:sum('qnodes'),nodes:off.nodes,ratio:sum('qnodes')/off.nodes,searched:sum('q_searched'),generated:sum('q_generated'),checks:sum('q_checks'),evasions:sum('qcheck_evasions'),stand_cutoffs:sum('q_stand_cutoffs'),see_skips:sum('q_see_prunes'),delta_skips:sum('q_delta_prunes'),note:'q_generated counts the legal candidate list, not captures only. No causal chain reconstruction; categories UNKNOWN.'},
 see:{status:'MEASURED_REFERENCE_SEE_RETAINED',off:off.counters.see,on:on.counters.see,origins:off.sites.see,pushes:off.sites.push.see,unmeasured:['mean/max recursive depth'],fast_see:'NOT_IMPLEMENTED'},
 movepicker:{status:'CANDIDATE_DEFAULT_OFF',feature:'reuse_move_facts',off:{check:off.counters.gives_check,scoring:off.counters.order_scoring,sort:off.counters.order_sort},on:{check:on.counters.gives_check,scoring:on.counters.order_scoring,sort:on.counters.order_sort},decision:'Reuse exact existing check/SEE facts; preserve ordering. Staged picker and history buckets deferred; sort is not dominant.'},
 safety:{status:'UNCHANGED_PRIOR_EVIDENCE_ONLY',evidence:['results/v06-safety-300/summary.json'],note:'300 FEN x 9 masks x 20000 nodes. All no-* masks ablate ALL, not legacy. No causal per-reason node attribution, no default mask change.'},
 eval:{status:'PARTIAL_UNCHANGED',evaluation:off.counters.evaluation,attack_map:off.counters.attack_map,king:off.counters.king_eval,pawn_probe:off.counters.pawn_probe,pawn_compute:off.counters.pawn_compute,pawn_hits:sum('pawn_cache_hits'),pawn_misses:sum('pawn_cache_misses'),pawn_hit_rate:sum('pawn_cache_hits')/(sum('pawn_cache_hits')+sum('pawn_cache_misses')),unmeasured:['replacement count','collision estimate','term correlation/leave-one-out','fine-grained term runtime']},
 tt:{status:'PARTIAL_UNCHANGED',proof_key:off.counters.proof_key,probes:sum('tt_probes'),hits:sum('tt_hits'),note:'proof_key timing is not TT context equality time. No compact-context replacement; bandwidth and allocation costs unmeasured.'},
 time:{status:'PARTIAL',evidence:['results/v07-stop-latency.json'],note:'External stop/IPC timing, subsystem UNKNOWN. TimeGuard unchanged OFF; adaptive_time OFF.'},
 'fixed-depth':{status:'MEASURED_SMALL_SAMPLE',data:benchmark.filter(r=>r.mode==='depth')},
 'fixed-nodes':{status:'MEASURED',data:benchmark.filter(r=>r.mode==='nodes'),evidence:['results/v07-final-off-equivalence.json','results/v07-move-facts-equivalence.json']},
 'fixed-time':{status:'MEASURED_SMALL_SAMPLE',data:benchmark.filter(r=>r.mode==='time'),quality:await read('results/v07-candidate-a/fixed-time-quality.json')},
 rejected:{status:'NO_NEW_REJECTION_OR_PROMOTION',prior_default_off:['partial_ordering','rfp','calibrated_lmr','adaptive_time'],deferred_not_failed:['incremental undo','fast SEE','direct legal generation','staged MovePicker','compact TT'],candidate:'reuse_move_facts remains OFF pending larger strength evidence'}
};
const provenance={date:new Date().toISOString(),command:process.argv,compiler:'MSVC 19.51.36246 Release AVX2 IPO (research profiles explicitly separated)',threads:1,hash_mb:32,source_revision:'No git; baseline/v07-candidate-source.zip SHA256',source_sha256:await sha('baseline/v07-candidate-source.zip'),production_sha256:await sha('build/Release/axiom-0.7-cost.exe'),benchmark_manifest:await read('results/v07-candidate-a/manifest.json'),profile_manifest:await read('results/v07-profile-off/manifest.json')};
for(const [name,data] of Object.entries(groups)) {
 const dir=`results/v07-${name}`;await mkdir(dir);
 await writeFile(`${dir}/manifest.json`,JSON.stringify({...provenance,...data},null,2),{flag:'wx'});
}
await mkdir('results/v07-qsearch-explosions');
const cases=obs.filter(r=>r.qsearch_ratio>0.9).map(r=>({fen:r.fen,ratio:r.qsearch_ratio,category:'UNKNOWN',qply_histogram:r.qply_histogram,stats:r.search_stats}));
await writeFile('results/v07-qsearch-explosions/manifest.json',JSON.stringify({...provenance,threshold:0.9,classification:'Heuristic screening only; high ratio is not proof of wasted work.',cases},null,2),{flag:'wx'});
console.log(`Created ${Object.keys(groups).length} evidence manifests; ${cases.length} qsearch ratio flags`);
