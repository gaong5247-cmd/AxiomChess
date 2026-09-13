#include "axiom/engine.hpp"
#include <sstream>
namespace axiom {
namespace {
std::string escaped(const std::string& s) { std::string out="\""; for(unsigned char c:s) {
    if(c=='"' || c=='\\') { out+='\\'; out+=static_cast<char>(c); } else if(c=='\n') out+="\\n"; else if(c>=32) out+=static_cast<char>(c);
    } return out+'"'; }
}
std::string json(const SearchResult& r,const Board& b) {
    std::ostringstream o; o<<"{\n  \"fen\":"<<escaped(b.fen())<<",\n  \"bestmove\":"<<escaped(r.best.uci())<<",\n  \"result\":"<<escaped(status_name(r.status))<<",\n  \"outcome\":"<<escaped(r.outcome)<<",\n  \"score_cp\":"<<r.score<<",\n  \"depth\":"<<r.depth<<",\n  \"nodes\":"<<r.nodes<<",\n  \"proof_nodes\":"<<r.proof_nodes<<",\n  \"elapsed_ms\":"<<r.elapsed_ms<<",\n  \"claim_draw\":"<<(r.claim_draw?"true":"false")<<",\n  \"verification\":{\"completed\":"<<(r.verification_completed?"true":"false")<<",\"certificate_verified\":"<<(r.certificate_verified?"true":"false")<<",\"detail\":"<<escaped(r.verification_detail)<<"},\n  \"evaluation\":{\"perspective\":\"side_to_move\",\"total\":"<<r.evaluation.total<<",\"phase\":"<<r.evaluation.phase;
    for(const auto& [name,value]:r.evaluation.terms) o<<','<<escaped(name)<<':'<<value; o<<"},\n  \"moves\":[";
    bool first=true;
    for(const auto& m:r.moves) { if(!first) o<<','; first=false;
        o<<"\n    {\"move\":"<<escaped(m.move.uci())<<",\"evaluation\":"<<m.score<<",\"depth\":"<<m.depth<<",\"proven_status\":"<<escaped(status_name(m.status))<<",\"mate_distance\":";
        if(m.mate_distance<0) o<<"null"; else o<<m.mate_distance;
        o<<",\"bound_type\":"<<escaped(bound_name(m.bound))<<",\"nodes\":"<<m.nodes<<",\"principal_variation\":[";
        for(std::size_t i=0;i<m.pv.size();++i) { if(i) o<<','; o<<escaped(m.pv[i].uci()); } o<<"],\"tactical_status\":[";
        for(std::size_t i=0;i<m.tactics.size();++i) { if(i) o<<','; o<<escaped(m.tactics[i]); }
        o<<"],\"tactical_verification\":{\"completed\":"<<(m.tactical_verified?"true":"false")<<",\"depth\":"<<m.verification_depth<<",\"score\":"<<m.verification_score<<",\"strongest_reply\":"<<escaped(m.strongest_reply.uci())<<"},\"tablebase_result\":";
        if(m.tablebase) o<<"{\"wdl\":"<<m.tablebase->wdl<<",\"dtz\":"<<m.tablebase->dtz<<'}'; else o<<"null";
        o<<'}';
    }
    o<<"\n  ],\n  \"tablebase\":";
    if(r.tablebase) o<<"{\"wdl\":"<<r.tablebase->wdl<<",\"dtz\":"<<r.tablebase->dtz<<",\"exact\":"<<(r.tablebase->exact?"true":"false")<<",\"reason\":"<<escaped(r.tablebase->reason)<<'}'; else o<<"null";
    o<<",\n  \"features\":"<<escaped(r.features)<<",\n  \"search_stats\":{\"threads\":"<<r.stats.threads<<",\"root_correction\":"<<r.stats.root_correction;
#define AXIOM_JSON_STAT(field) o<<",\"" #field "\":"<<r.stats.field
    AXIOM_JSON_STAT(correction_updates); AXIOM_JSON_STAT(continuation_updates); AXIOM_JSON_STAT(capture_updates);
    AXIOM_JSON_STAT(tt_hits); AXIOM_JSON_STAT(null_attempts); AXIOM_JSON_STAT(null_verifications); AXIOM_JSON_STAT(null_rejections);
    AXIOM_JSON_STAT(singular_tests); AXIOM_JSON_STAT(singular_extensions); AXIOM_JSON_STAT(probcut_attempts); AXIOM_JSON_STAT(probcut_cutoffs);
    AXIOM_JSON_STAT(lmr_reductions); AXIOM_JSON_STAT(lmr_researches); AXIOM_JSON_STAT(history_prunes); AXIOM_JSON_STAT(see_prunes);
    AXIOM_JSON_STAT(mate_distance_cutoffs); AXIOM_JSON_STAT(iid_searches); AXIOM_JSON_STAT(bestmove_changes);
    AXIOM_JSON_STAT(safety_protected); AXIOM_JSON_STAT(pawn_cache_hits); AXIOM_JSON_STAT(pawn_cache_misses);
    AXIOM_JSON_STAT(qnodes); AXIOM_JSON_STAT(tt_cutoffs); AXIOM_JSON_STAT(null_cutoffs); AXIOM_JSON_STAT(pvs_scouts); AXIOM_JSON_STAT(pvs_researches);
    AXIOM_JSON_STAT(lmr_verifications_completed); AXIOM_JSON_STAT(horizon_interruptions); AXIOM_JSON_STAT(qcheck_evasions);
    AXIOM_JSON_STAT(rfp_cuts);
#define AXIOM_JSON_CAL(name) AXIOM_JSON_STAT(name);
    AXIOM_CALIBRATION_FIELDS(AXIOM_JSON_CAL)
#undef AXIOM_JSON_CAL
#undef AXIOM_JSON_STAT
    o<<"},\n\"safety_mask\":"<<r.safety_mask<<",\n\"force_root_move\":"<<escaped(r.force_root_move)<<",\n\"safety_skipped_checks\":{";
    for(unsigned a=0;a<8;++a) {
        if(a) o<<','; o<<escaped(std::string(SafetyActionNames[a]))<<":{";
        for(unsigned reason=0;reason<14;++reason) { if(reason) o<<','; o<<escaped(std::string(SafetyNames[reason]))<<':'<<r.stats.safety.skipped_checks[a][reason]; }
        o<<'}';
    }
    o<<"},\n\"qply_histogram\":[";
    for(unsigned q=0;q<MaxPly;++q) { if(q) o<<','; o<<r.stats.qply_histogram[q]; }
    o<<"],\n\"time_telemetry\":{\"queue_ms\":"<<r.queue_ms<<",\"setup_ms\":"<<r.setup_ms<<",\"hard_overshoot_ms\":"<<r.hard_overshoot_ms
     <<",\"next_iteration_estimate_ms\":"<<r.next_iteration_estimate_ms<<",\"last_completed_estimate_error_ms\":"<<r.iteration_estimate_error_ms
     <<",\"guard_stopped\":"<<(r.time_guard_stopped?"true":"false")<<"},\n\"qsearch_ratio\":"<<(r.nodes?double(r.stats.qnodes)/r.nodes:0)<<",\n\"correction_samples\":[";
    for(std::size_t i=0;i<r.correction_samples.size();++i) {
        if(i) o<<','; const auto& s=r.correction_samples[i];
        o<<"{\"raw\":"<<s.raw<<",\"corrected\":"<<s.corrected<<",\"score\":"<<s.score<<",\"pieces\":"<<s.pieces<<",\"components_scaled16\":[";
        for(int j=0;j<5;++j) { if(j) o<<','; o<<s.components[j]; } o<<"]}";
    }
    o<<"],\n\"lmr_samples\":[";
    for(std::size_t i=0;i<r.lmr_samples.size();++i) {
        if(i) o<<','; const auto& s=r.lmr_samples[i];
        o<<"{\"fen\":"<<escaped(s.fen)<<",\"move\":"<<escaped(s.move.uci())<<",\"root_move\":"<<escaped(s.root_move.uci())
         <<",\"depth\":"<<s.depth<<",\"reduced_depth\":"<<s.depth-1-s.reduction<<",\"reduction\":"<<s.reduction<<",\"index\":"<<s.index
         <<",\"history\":"<<s.history<<",\"continuation\":"<<s.continuation<<",\"capture_history\":"<<s.capture_history<<",\"corrected_eval\":"<<s.eval
         <<",\"trend\":"<<s.trend<<",\"reduced_score\":"<<s.reduced_score<<",\"score\":"<<s.score<<",\"researched\":"<<(s.researched?"true":"false")<<'}';
    }
    o<<"],\n\"time_factor\":"<<r.time_factor<<",\"root_node_fraction\":"<<r.root_node_fraction<<",\"root_history\":[";
    for(std::size_t i=0;i<r.root_history.size();++i) {
        if(i) o<<','; const auto& h=r.root_history[i];
        o<<"{\"move\":"<<escaped(h.move.uci())<<",\"score\":"<<h.score<<",\"previous_score\":"<<h.previous_score<<",\"depth\":"<<h.depth
         <<",\"observations\":"<<h.observations<<",\"stable_iterations\":"<<h.stable_iterations<<",\"nodes\":"<<h.nodes
         <<",\"score_variance\":"<<(h.observations>1?h.m2/(h.observations-1):0)<<'}';
    }
    o<<"]\n";
#ifdef AXIOM_RESEARCH_TRACE
    if(r.hot_profile) {o<<",\"hot_profile\":";r.hot_profile->json(o);}
#endif
    o<<"}\n"; return o.str();
}
}
