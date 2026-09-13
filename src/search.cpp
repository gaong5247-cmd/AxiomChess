#include "axiom/engine.hpp"
#include "search_internal.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef AXIOM_RESEARCH_TRACE
#include <filesystem>
#endif

namespace axiom {
using search_detail::Applied;
using search_detail::Interrupted;
namespace {

bool null_material(const Board& b) {
    int count=0, nonpawns=0;
    for(int s=0;s<128;++s) if(valid(s) && color(b.squares[s])==b.side) {
        int pt=std::abs(b.squares[s]); if(pt!=Pawn && pt!=King) { nonpawns+=piece_value(pt); ++count; }
    }
    return count>=2 && nonpawns>=850 && b.piece_count()>10 && b.halfmove<80;
}

}
Search::Search(std::size_t mb):tt_(std::make_shared<TranspositionTable>(mb)),feedback_(std::make_unique<Feedback>()),pawn_cache_(std::make_unique<PawnCache>()) {}
void Search::clear() { tt_->clear(); feedback_->clear(); pawn_cache_->clear(); for(auto& row:killers_) for(auto& m:row) m=Move{}; for(auto& side:history_) for(auto& from:side) for(auto& to:from) to=0; trace_.fill(SearchTrace{}); }
int Search::static_evaluation(const Board& b) { ProfileScope timer(limits_.profile,stats_.eval_ns); return evaluate_score(b,limits_.features.strategic_eval,limits_.features.king_safety,limits_.features.pawn_cache?pawn_cache_.get():nullptr); }
void Search::tick() {
    if(stop_->load() || (local_stop_ && local_stop_->load()) || std::chrono::steady_clock::now()>=deadline_) {
#ifdef AXIOM_RESEARCH_TRACE
        if(causal_trace_) { CausalEvent e;e.reason=stop_->load()?"STOP_INTERRUPTION":"TIME_OR_WORKER_INTERRUPTION";e.node_id=causal_trace_->active;causal_trace_->emit(std::move(e),nodes_); }
#endif
        throw Interrupted{};
    }
    if(shared_nodes_) {
        auto previous=shared_nodes_->fetch_add(1);
        if(limits_.nodes && previous>=limits_.nodes) { shared_nodes_->fetch_sub(1); throw Interrupted{}; }
    } else if(limits_.nodes && nodes_>=limits_.nodes) throw Interrupted{};
    ++nodes_;
}
int Search::previous_token(int ply,int distance) const { if(ply<distance) return 0; const auto& prev=trace_[ply-distance]; return Feedback::token(prev.piece,prev.move.to); }
int Search::history_score(const Board& b,Move m,int ply) const {
    int score=history_[b.side==White?0:1][m.from][m.to];
    if(limits_.features.continuation) { int current=Feedback::token(b.squares[m.from],m.to),i=0;
        for(int distance:{1,2,4,6}) score+=feedback_->continuation(previous_token(ply,distance),current,i++)/2;
    } return score;
}
int Search::corrected_eval(const Board& b,int raw,int ply) const {
    AXIOM_HOT(Correction,Inherit);
    ProfileScope timer(limits_.profile,stats_.correction_ns);
    int value=raw+(limits_.features.correction?feedback_->correction(b,previous_token(ply),limits_.correction_mask):0);
    return std::clamp(value,-MateThreshold+1,MateThreshold-1);
}
CorrectionSample Search::frozen_correction(const Board& b,int previous) const {
    CorrectionSample sample;
    sample.raw=evaluate_score(b,limits_.features.strategic_eval,limits_.features.king_safety,nullptr);
    sample.corrected=std::clamp(sample.raw+feedback_->correction(b,previous,limits_.correction_mask),-MateThreshold+1,MateThreshold-1);
    sample.components=feedback_->correction_components(b,previous); sample.pieces=b.piece_count();
    return sample;
}
int Search::negamax(Board& b,int depth,int alpha,int beta,int ply,bool pv,bool null_allowed,std::vector<Move>& line,bool synthetic,Move excluded,bool auxiliary,int extensions,bool cut_node) {
    AXIOM_HOT(MainSearch,Search);
    AXIOM_TRACE_SCOPE(causal_trace_.get(),b,depth,alpha,beta,ply,-1,trace_[0].move.uci(),pv,synthetic,auxiliary,nodes_);
    line.clear();
    if(depth<=0) return quiescence(b,alpha,beta,ply,0,synthetic,auxiliary);
    tick(); stats_.seldepth=std::max(stats_.seldepth,ply);
    auto moves=[&] { ProfileScope timer(limits_.profile,stats_.legal_ns); return b.legal_moves(); }(); bool check=b.in_check();
    if(moves.empty()) return check?-MateScore+ply:0;
    if(synthetic?b.insufficient():b.automatic_draw()) return 0;
    bool claim=!synthetic && b.can_claim_draw(true);
    if(ply>=MaxPly-1) { if(check) { ++stats_.horizon_interruptions; throw Interrupted{}; } int value=static_evaluation(b); return claim?std::max(0,value):value; }
    const auto& f=limits_.features;
    if(f.mate_distance) {
        alpha=std::max(alpha,-MateScore+ply);
        beta=std::min(beta,MateScore-ply-1);
        if(alpha>=beta) { ++stats_.mate_distance_cutoffs; AXIOM_TRACE_EVENT("MATE_DISTANCE_CUTOFF",Move{},alpha); return alpha; }
    }
    int original_alpha=alpha;
    if(claim) { if(beta<=0) return 0; alpha=std::max(alpha,0); }
    const bool trusted_context=!synthetic && !auxiliary && !excluded;
    const bool pruning=trusted_context && limits_.selective;
    auto key=b.hash(); auto context=b.proof_key();
    auto entry=[&] { ProfileScope timer(limits_.profile,stats_.tt_ns);
        if(trusted_context && limits_.use_tt) { ++stats_.tt_probes; return tt_->probe(key,context); }
        return std::optional<TTEntry>{}; }();
    Move ttmove;
    if(entry) {
        ++stats_.tt_hits; ttmove=entry->move;
        int value=score_from_tt(entry->score,ply);
        AXIOM_TRACE_DETAIL(causal.base.tt_score=value; causal.base.tt_depth=entry->depth; causal.base.tt_bound=bound_name(entry->bound));
        AXIOM_TRACE_EVENT("TT_HIT",ttmove,value);
        if(!pv && entry->depth>=depth && (entry->bound==Bound::Exact || (entry->bound==Bound::Lower && value>=beta) || (entry->bound==Bound::Upper && value<=alpha))) { ++stats_.tt_cutoffs; AXIOM_TRACE_EVENT("TT_CUTOFF",ttmove,value); return value; }
    }
    int raw=static_evaluation(b);
    int static_eval=trusted_context?corrected_eval(b,raw,ply):raw;
    trace_[ply].static_eval=check?Infinity:static_eval;
    bool improving=!check && ply>=2 && trace_[ply-2].static_eval!=Infinity && static_eval>trace_[ply-2].static_eval;
    bool unstable=ply>=2 && trace_[ply-2].static_eval!=Infinity && std::abs(static_eval-trace_[ply-2].static_eval)>=SearchTuning::UnstableEvalDelta;
    const int trend=eval_trend(check?Infinity:static_eval,ply>=2?trace_[ply-2].static_eval:Infinity);
    AXIOM_TRACE_DETAIL(causal.base.raw=raw; causal.base.corrected=static_eval; causal.base.trend=trend; causal.base.correction=feedback_->correction_components(b,previous_token(ply)));
    AXIOM_TRACE_EVENT("STATIC_EVALUATION",Move{},static_eval);
    // One bounded uncertainty gate, shared across heuristics; never compound aggression.
    if(f.trend_safety && std::abs(trend)>=80) unstable=true;
    PawnInfo safety_pawns; bool tactical_threat=false;
    if(f.search_safety) {
        safety_pawns=pawn_cache_->probe(b);
        for(int s=0;s<128;++s) if(valid(s) && b.squares[s]==b.side*Queen && b.attacked(s,-b.side)) tactical_threat=true;
    }
    const unsigned node_safety=f.search_safety?limits_.safety_mask &
        ((improving?SafetyImproving:0u)|(unstable?SafetyUnstable:0u)|(tactical_threat?SafetyTactical:0u)):0;
    AXIOM_TRACE_DETAIL(causal.base.safety=node_safety);
    bool king_danger=false;
    if(f.search_safety && (limits_.safety_mask & SafetyKing)) {
        for(int sq=0;sq<128;++sq) if(valid(sq) && b.squares[sq]==b.side*King)
            for(int delta:{-17,-16,-15,-1,1,15,16,17}) if(valid(sq+delta) && b.attacked(sq+delta,-b.side)) king_danger=true;
    }
    if(pruning && f.iid && !ttmove && depth>=5) {
        AXIOM_TRACE_EVENT("IID_BEGIN",Move{},std::nullopt);
        auto before=nodes_;
        ++stats_.iid_searches; std::vector<Move> seed;
        negamax(b,depth-2,alpha,beta,ply,true,false,seed,false,{},true,extensions,false);
        stats_.iid_nodes+=nodes_-before;
        if(!seed.empty()) { ttmove=seed.front(); ++stats_.iid_moves_found; }
        AXIOM_TRACE_EVENT("IID_END",ttmove,std::nullopt);
    }
    int singular_extension=0;
    if(pruning && f.singular && !check && entry && ttmove && entry->move==ttmove && depth>=6 && entry->depth>=depth-2 && entry->bound!=Bound::Upper && std::abs(entry->score)<MateThreshold && extensions<2) {
        ++stats_.singular_tests; int threshold=score_from_tt(entry->score,ply)-2*depth;
        std::vector<Move> ignored;
        auto before=nodes_;
        int alternative=negamax(b,(depth-1)/2,threshold-1,threshold,ply,false,false,ignored,false,ttmove,true,extensions,false);
        AXIOM_TRACE_EVENT("SINGULAR_EXCLUDED_RESULT",ttmove,alternative);
        stats_.singular_excluded_nodes+=nodes_-before;
        if(alternative<threshold) { singular_extension=1; ++stats_.singular_extensions; AXIOM_TRACE_EVENT("SINGULAR_EXTENSION",ttmove,alternative); }
    }
    if(pruning && f.rfp && !pv && !check && depth<=SearchTuning::RfpMaxDepth) ++stats_.rfp_attempts;
    if(pruning && f.rfp && !pv && !check && !claim && !ttmove && !unstable && !tactical_threat && moves.size()>1 &&
       depth<=SearchTuning::RfpMaxDepth && null_material(b) && std::abs(beta)<MateThreshold-512 && std::abs(static_eval)<MateThreshold-512) {
        int margin=SearchTuning::RfpBaseMargin+SearchTuning::RfpDepthMargin*depth;
        if(static_eval-margin>=beta) {
            bool tactical=false;
            for(auto m:moves) if(b.capture(m) || m.promotion || b.gives_check(m)) { tactical=true; break; }
            if(!tactical) for(int sq=0;sq<128;++sq) if(valid(sq) && color(b.squares[sq])==b.side &&
                std::abs(b.squares[sq])>=Knight && b.attacked(sq,-b.side)) { tactical=true; break; }
            if(!tactical) { ++stats_.rfp_cuts; AXIOM_TRACE_EVENT("RFP_CUTOFF",Move{},static_eval-margin); return static_eval-margin; }
        }
    }
    if(pruning && null_allowed && !pv && !check && depth>=3 && !null_material(b)) ++stats_.null_material_rejections;
    if(pruning && limits_.use_null && null_allowed && !pv && !check && moves.size()>1 && depth>=3 && null_material(b) && std::abs(beta)<MateThreshold && (!(f.verified_null || f.correction) || static_eval>=beta) && node_safety)
        { stats_.safety.record(SafetyAction::Null,node_safety); AXIOM_TRACE_EVENT("NULL_BLOCKED_BY_SAFETY",Move{},std::nullopt); }
    if(pruning && limits_.use_null && null_allowed && !pv && !check && moves.size()>1 && !(f.trend_safety && unstable) && !node_safety && depth>=3 && null_material(b) && std::abs(beta)<MateThreshold && (!(f.verified_null || f.correction) || static_eval>=beta)) {
        ++stats_.null_attempts; int reduced=std::max(0,depth-1-(2+depth/5)),score;
        AXIOM_TRACE_EVENT("NULL_BEGIN",Move{},std::nullopt);
        trace_[ply].move={}; trace_[ply].piece=0;
        { Applied applied(b,{}); std::vector<Move> ignored;
            score=-negamax(b,reduced,-beta,-beta+1,ply+1,false,false,ignored,true,{},true,extensions,!cut_node); }
        AXIOM_TRACE_EVENT("NULL_RESULT",Move{},score);
        if(score>=beta) {
            ++stats_.null_fail_highs;
            bool accept=true;
            if(f.verified_null && depth>=6) {
                ++stats_.null_verifications; std::vector<Move> ignored;
                auto before=nodes_;
                int verified=negamax(b,reduced,beta-1,beta,ply,false,false,ignored,false,{},true,extensions,cut_node);
                stats_.null_verification_nodes+=nodes_-before;
                if(verified>=beta) ++stats_.null_verification_success;
                accept=verified>=beta; if(!accept) ++stats_.null_rejections;
                AXIOM_TRACE_EVENT(accept?"NULL_VERIFICATION_SUCCESS":"NULL_VERIFICATION_FAILED",Move{},verified);
                score=std::min(score,verified);
            }
            if(accept) { ++stats_.null_cutoffs; AXIOM_TRACE_EVENT("NULL_CUTOFF",Move{},score); return std::min(score,MateThreshold-1); }
        }
    }
    order(b,moves,ttmove,ply);
    if(pruning && f.probcut && !pv && !check && moves.size()>1 && null_material(b) && depth>=5 && std::abs(beta)<MateThreshold-256 && static_eval>=beta-150 && node_safety)
        { stats_.safety.record(SafetyAction::Probcut,node_safety); AXIOM_TRACE_EVENT("PROBCUT_BLOCKED_BY_SAFETY",Move{},std::nullopt); }
    if(pruning && f.probcut && !pv && !check && moves.size()>1 && null_material(b) && !(f.trend_safety && unstable) && !node_safety && depth>=5 && std::abs(beta)<MateThreshold-256 && static_eval>=beta-150) {
        int raised=beta+SearchTuning::ProbCutBaseMargin+(improving?SearchTuning::ProbCutImprovingMargin:0);
        for(auto m:moves) if(b.capture(m) && !m.promotion && see(b,m)>=0) {
            ++stats_.probcut_attempts; int score; auto before=nodes_;
            AXIOM_TRACE_EVENT("PROBCUT_BEGIN",m,std::nullopt);
            trace_[ply].move=m; trace_[ply].piece=b.squares[m.from];
            { Applied applied(b,m); score=-quiescence(b,-raised,-raised+1,ply+1,0,false,true);
                if(score>=raised) { ++stats_.probcut_qsuccess; std::vector<Move> ignored; score=-negamax(b,depth-4,-raised,-raised+1,ply+1,false,false,ignored,false,{},true,extensions,true); if(score>=raised) ++stats_.probcut_reduced_success; }
            }
            stats_.probcut_nodes+=nodes_-before;
            AXIOM_TRACE_EVENT("PROBCUT_RESULT",m,score);
            if(score>=raised && std::abs(score)<MateThreshold) { ++stats_.probcut_cutoffs; AXIOM_TRACE_EVENT("PROBCUT_CUTOFF",m,score); return score-(raised-beta); }
        }
    }
    trace_[ply].static_eval=check?Infinity:static_eval;
    int best=claim?0:-Infinity,index=0; Move bestmove; bool best_quiet=false;
    std::vector<Move> quiets,captures;
    auto update_move=[&](Move m,int bonus) {
        if(!trusted_context) return;
        if(b.capture(m)) {
            if(f.capture_history) { feedback_->update_capture(b.squares[m.from],m.to,b.squares[m.to]?std::abs(b.squares[m.to]):Pawn,bonus); ++stats_.capture_updates; }
        } else {
            auto& h=history_[b.side==White?0:1][m.from][m.to];
            h=std::clamp(h+bonus-h*std::abs(bonus)/16000,-16000,16000);
            if(f.continuation) { int current=Feedback::token(b.squares[m.from],m.to),i=0;
                for(int distance:{1,2,4,6}) { int previous=previous_token(ply,distance); feedback_->update_continuation(previous,current,i++,bonus); if(previous) ++stats_.continuation_updates; }
            }
        }
    };
    for(std::size_t ordered_index=0;auto m:moves) {
        const MoveFacts* facts=f.reuse_move_facts && move_facts_ && moves.size()<=256?&move_facts_[ply*256+ordered_index]:nullptr;
        ++ordered_index;
        if(m==excluded) continue;
        bool cap=b.capture(m),quiet=!cap && !m.promotion && !(facts?facts->checking:b.gives_check(m));
        bool killer=m==killers_[ply][0] || m==killers_[ply][1];
        int hist=history_score(b,m,ply),extension=m==ttmove?singular_extension:0;
        const bool counter=f.countermove && m==feedback_->counter(previous_token(ply));
        const int continuation=f.continuation?feedback_->continuation(previous_token(ply),Feedback::token(b.squares[m.from],m.to),0):0;
        SafetySignals signals{pv,m==ttmove,check || (facts?facts->checking:b.gives_check(m)),bool(m.promotion),hist>2000,bool(extension),false,improving,tactical_threat};
        if(f.search_safety && index>=3) {
            signals.strategic=strategic_candidate(b,m,safety_pawns);
            signals.threat|=b.attacked(m.from,-b.side) && !b.attacked(m.from,b.side);
        }
        // King danger is deliberately a cheap ring-attack signal, not a probability.
        unsigned reasons=(signals.pv?SafetyPV:0u)|(signals.tt?SafetyTT:0u)|(signals.check?SafetyCheck:0u)|
            (signals.promotion?SafetyPromotion:0u)|(signals.high_history?SafetyHistory:0u)|
            (signals.singular?SafetySingular:0u)|(signals.strategic?SafetyStrategic:0u)|
            (signals.improving?SafetyImproving:0u)|(signals.threat?SafetyTactical:0u)|
            (unstable?SafetyUnstable:0u)|(moves.size()==1?SafetyOnly:0u)|
            (continuation>2000?SafetyContinuation:0u)|(counter?SafetyCounter:0u)|(king_danger?SafetyKing:0u);
        reasons=f.search_safety?reasons & limits_.safety_mask:0;
        bool protected_move=bool(reasons) || (f.trend_safety && unstable) ||
            (f.calibrated_lmr && (counter || continuation>2000 || unstable || moves.size()==1));
        AXIOM_TRACE_DETAIL(causal.base.alpha=alpha; causal.base.index=index; causal.base.history=hist; causal.base.continuation=continuation;
            causal.base.capture_history=cap?feedback_->capture_score(b.squares[m.from],m.to,b.squares[m.to]?std::abs(b.squares[m.to]):Pawn):0;
            causal.base.counter=counter; causal.base.capture=cap; causal.base.check=signals.check; causal.base.promotion=bool(m.promotion);
            causal.base.safety=reasons; causal.base.reduction=0; causal.base.see=see(b,m));
        AXIOM_TRACE_EVENT("MOVE_CONSIDERED",m,std::nullopt);
        if(protected_move) { AXIOM_TRACE_EVENT("SAFETY_PROTECTION",m,std::nullopt); }
        if(protected_move && index>=3) ++stats_.safety_protected;
        bool can_skip=pruning && !protected_move && !pv && !check && !extension && m!=ttmove && !killer && !m.promotion && !signals.check && index>=3 && best>-MateThreshold;
        if(pruning && reasons && !pv && !check && !extension && m!=ttmove && !killer && !m.promotion && !signals.check && index>=3 && best>-MateThreshold) {
            if(f.history_pruning && quiet && depth<=3 && hist<-1000*depth && !improving) stats_.safety.record(SafetyAction::History,reasons);
            if(f.see_pruning && depth<=3) stats_.safety.record(SafetyAction::See,reasons);
            AXIOM_TRACE_DETAIL(if(f.history_pruning && quiet && depth<=3 && hist<-1000*depth && !improving) causal.event("HISTORY_PRUNE_BLOCKED_BY_SAFETY",nodes_,m);
                if(f.see_pruning && depth<=3 && causal.base.see && *causal.base.see<-(cap?90:40)*depth*depth) causal.event("SEE_PRUNE_BLOCKED_BY_SAFETY",nodes_,m));
        }
        const bool poor_history=can_skip && f.history_pruning && quiet && depth<=3 && hist<-1000*depth && !improving;
        if(poor_history && !f.calibrated_lmr) { ++stats_.history_prunes; AXIOM_TRACE_EVENT("HISTORY_PRUNE_EXECUTED",m,std::nullopt); continue; }
        if(can_skip && f.see_pruning && depth<=3 && (facts && facts->see_known?facts->exchange:see(b,m))<-(cap?90:40)*depth*depth) { ++stats_.see_prunes; AXIOM_TRACE_EVENT("SEE_PRUNE_EXECUTED",m,std::nullopt); continue; }
        int reduction=0;
        if(pruning && limits_.use_lmr && limits_.pvs && reasons && !check && quiet && m!=ttmove && !killer && !extension && depth>=3 && index>=3 && (!pv || f.dynamic_lmr))
        {
            stats_.safety.record(SafetyAction::Lmr,reasons);
            AXIOM_TRACE_DETAIL(int prevented=1+static_cast<int>(std::log(double(depth))*std::log(double(index+1))/3)-(hist>2000);
                if(f.dynamic_lmr) prevented+=(cut_node?1:0)+(hist<-1500?1:0)-(improving?1:0)-(pv?1:0)-(entry && entry->pv?1:0)-(hist>6000?1:0);
                causal.base.reduction=std::clamp(prevented,0,depth-2); if(causal.base.reduction) causal.event("LMR_BLOCKED_BY_SAFETY",nodes_,m));
        }
        if(pruning && limits_.use_lmr && limits_.pvs && !protected_move && !check && quiet && m!=ttmove && !killer && !extension && depth>=3 && index>=3 && (!pv || f.dynamic_lmr)) {
            reduction=1+static_cast<int>(std::log(double(depth))*std::log(double(index+1))/3)-(hist>2000);
            if(f.dynamic_lmr) reduction+=(cut_node?1:0)+(hist<-1500?1:0)-(improving?1:0)-(pv?1:0)-(entry && entry->pv?1:0)-(hist>6000?1:0);
            reduction=std::clamp(reduction,0,depth-2); if(reduction) ++stats_.lmr_reductions;
        }
        stats_.lmr_reduction_sum+=reduction;
        if(poor_history && f.calibrated_lmr && limits_.pvs && limits_.use_lmr) {
            const int old=reduction; reduction=std::clamp(std::max(1,reduction),0,std::max(0,depth-2));
            if(!old && reduction) ++stats_.lmr_reductions;
            stats_.lmr_reduction_sum+=reduction-old; ++stats_.history_reductions;
        }
        const bool record=limits_.calibration_samples>0 && reduction>0 && lmr_samples_.size()<static_cast<std::size_t>(limits_.calibration_samples);
        AXIOM_TRACE_DETAIL(causal.base.reduction=reduction);
        if(reduction) { AXIOM_TRACE_EVENT("LMR_APPLIED",m,std::nullopt); }
        LmrSample diagnostic;
        if(record) diagnostic={b.fen(),m,trace_[0].move,depth,reduction,index,hist,continuation,cap?feedback_->capture_score(b.squares[m.from],m.to,b.squares[m.to]?std::abs(b.squares[m.to]):Pawn):0,static_eval,trend};
        auto branch_start=nodes_;
        std::vector<Move> child; int score,child_depth=depth-1+extension;
        trace_[ply].move=m; trace_[ply].piece=b.squares[m.from];
        { Applied applied(b,m);
            if(index==0 || !limits_.pvs) score=-negamax(b,child_depth,-beta,-alpha,ply+1,pv,true,child,synthetic,{},auxiliary,extensions+extension,!cut_node);
            else {
                ++stats_.pvs_scouts;
                AXIOM_TRACE_EVENT("PVS_SCOUT_BEGIN",m,std::nullopt);
                score=-negamax(b,child_depth-reduction,-alpha-1,-alpha,ply+1,false,true,child,synthetic,{},auxiliary,extensions+extension,true);
                AXIOM_TRACE_EVENT(reduction?"LMR_REDUCED_RESULT":"PVS_SCOUT_RESULT",m,score);
                if(record) { diagnostic.reduced_score=score; diagnostic.researched=reduction && score>alpha; }
                if(reduction && score<=alpha) ++stats_.lmr_unverified_fail_lows;
                if(reduction && score>alpha) { ++stats_.lmr_researches; AXIOM_TRACE_EVENT("LMR_RESEARCH_TRIGGERED",m,score); score=-negamax(b,child_depth,-alpha-1,-alpha,ply+1,false,true,child,synthetic,{},auxiliary,extensions+extension,true); ++stats_.lmr_verifications_completed; AXIOM_TRACE_EVENT("LMR_FULL_DEPTH_RESULT",m,score); }
                if(score>alpha && score<beta) { ++stats_.pvs_researches; score=-negamax(b,child_depth,-beta,-alpha,ply+1,pv,true,child,synthetic,{},auxiliary,extensions+extension,false); }
            }
        }
        AXIOM_TRACE_EVENT("MOVE_RESULT",m,score);
        ++index;
        if(record && lmr_samples_.size()<static_cast<std::size_t>(limits_.calibration_samples)) { diagnostic.score=score; lmr_samples_.push_back(std::move(diagnostic)); }
        if(extension) { stats_.singular_branch_nodes+=nodes_-branch_start; if(score>=beta) ++stats_.singular_beta_cutoffs; if(score>best) ++stats_.singular_pv_updates; }
        if(score>best) { best=score; bestmove=m; best_quiet=quiet; line=child; line.insert(line.begin(),m); }
        alpha=std::max(alpha,score);
        if(alpha>=beta) {
            AXIOM_TRACE_EVENT("BETA_CUTOFF",m,score);
            int bonus=std::min(1500,16*depth*depth); update_move(m,bonus);
            for(auto failed:quiets) update_move(failed,-bonus/2);
            for(auto failed:captures) update_move(failed,-bonus/2);
            if(quiet && trusted_context) { killers_[ply][1]=killers_[ply][0]; killers_[ply][0]=m;
                if(f.countermove) feedback_->set_counter(previous_token(ply),m); }
            break;
        }
        if(cap) captures.push_back(m); else if(quiet) quiets.push_back(m);
    }
    Bound bound=best<=original_alpha?Bound::Upper:best>=beta?Bound::Lower:Bound::Exact;
    if(trusted_context && !check && !claim && best_quiet && bound==Bound::Exact && std::abs(best)<MateThreshold &&
       correction_samples_.size()<static_cast<std::size_t>(std::clamp(limits_.calibration_samples,0,4096))) {
        // Pre-update prediction, finite search label; not an external truth oracle.
        correction_samples_.push_back({raw,static_eval,best,b.piece_count(),feedback_->correction_components(b,previous_token(ply))});
    }
    if(trusted_context) {
        if(limits_.use_tt) tt_->store({key,std::move(context),depth,score_to_tt(best,ply),bound,bestmove,pv},f.tt_policy);
        if(f.correction && !check && !claim && bestmove && best_quiet && std::abs(best)<MateThreshold &&
           (bound==Bound::Exact || (bound==Bound::Lower && best>static_eval) || (bound==Bound::Upper && best<static_eval))) {
            feedback_->update_correction(b,previous_token(ply),best-raw,depth); ++stats_.correction_updates;
        }
    }
    return best;
}
std::vector<MoveResult> Search::root(Board& b,int depth,int alpha,int beta,bool independent) {
    auto moves=b.legal_moves(); auto entry=limits_.use_tt?tt_->probe(b.hash(),b.proof_key()):std::optional<TTEntry>{};
    if(!limits_.force_root_move.empty()) moves.erase(std::remove_if(moves.begin(),moves.end(),[&](Move m){return m.uci()!=limits_.force_root_move;}),moves.end());
    order(b,moves,entry?entry->move:Move{},0);
    std::vector<MoveResult> results; results.reserve(moves.size()); int index=0;
    const bool root_check=b.in_check();
    const int root_raw=root_check?0:static_evaluation(b);
    for(auto m:moves) {
        auto before=nodes_; int a=alpha; std::vector<Move> child; int score;
        AXIOM_TRACE_SCOPE(causal_trace_.get(),b,depth,alpha,beta,0,-1,m.uci(),true,false,false,nodes_);
        AXIOM_TRACE_DETAIL(causal.base.index=index);
        AXIOM_TRACE_EVENT("ROOT_SEARCH_BEGIN",m,std::nullopt);
        trace_[0].move=m; trace_[0].piece=b.squares[m.from];
        trace_[0].static_eval=root_check?Infinity:corrected_eval(b,root_raw,0);
        { Applied applied(b,m);
            if(index==0 || independent || !limits_.pvs) score=-negamax(b,depth-1,-beta,-alpha,1,true,true,child);
            else { ++stats_.pvs_scouts; score=-negamax(b,depth-1,-alpha-1,-alpha,1,false,true,child);
                if(score>alpha && score<beta) { ++stats_.pvs_researches; score=-negamax(b,depth-1,-beta,-alpha,1,true,true,child); } }
        }
        MoveResult result; result.move=m; result.score=score; result.depth=depth; result.status=Status::SearchResult;
        AXIOM_TRACE_EVENT("ROOT_SEARCH_RESULT",m,score);
        result.bound=score<=a?Bound::Upper:score>=beta?Bound::Lower:Bound::Exact; result.nodes=nodes_-before;
        result.pv=child; result.pv.insert(result.pv.begin(),m); results.push_back(result);
        if(!independent) alpha=std::max(alpha,score); ++index;
        if(alpha>=beta) break;
    }
    std::stable_sort(results.begin(),results.end(),[](const auto& a,const auto& c){return a.score>c.score;});
    return results;
}
SearchResult Search::run(Board b,Limits limits,std::atomic_bool& stop,const std::function<void(const SearchResult&)>& info) {
#ifdef AXIOM_RESEARCH_TRACE
    if(limits.cost_profile && (limits.threads!=1 || limits.trace_search)) throw std::invalid_argument("Cost profiling requires Threads=1 and tracing OFF");
    HotSession hot_session(limits.cost_profile);
    causal_trace_.reset();
    if(limits.replay_subtree) {
        if(limits.threads!=1 || !limits.force_root_move.empty() || limits.verify) throw std::invalid_argument("Subtree replay requires Threads=1, no root constraint or verification");
        if(limits.replay_alpha>=limits.replay_beta || limits.replay_alpha<-Infinity || limits.replay_beta>Infinity) throw std::invalid_argument("Invalid subtree replay window");
        limits.proof_nodes=0;
    }
    if(limits.trace_search) {
        if(limits.threads!=1) throw std::invalid_argument("Causal tracing requires Threads=1");
        if(limits.trace_max_events<1 || limits.trace_max_events>1000000) throw std::invalid_argument("Invalid trace event cap");
        if(limits.trace_output.empty()) throw std::invalid_argument("--trace-output is required");
        if(!limits.trace_root_move.empty() && !b.parse_move(limits.trace_root_move)) throw std::invalid_argument("Illegal trace root move");
        if(std::filesystem::exists(limits.trace_output)) throw std::invalid_argument("Refusing to overwrite trace output");
        std::ofstream reserved(limits.trace_output);
        if(!reserved) throw std::invalid_argument("Cannot create trace output");
        reserved<<"{\"reason\":\"TRACE_MANIFEST\",\"schema\":1,\"root_fen\":"<<trace_quote(b.fen())<<",\"features\":"<<trace_quote(limits.features.names())
                <<",\"safety_mask\":"<<limits.safety_mask<<",\"max_events\":"<<limits.trace_max_events<<",\"node_budget\":"<<limits.nodes
                <<",\"depth_limit\":"<<limits.depth<<",\"time_limit_ms\":"<<limits.milliseconds<<",\"root_filter\":"<<trace_quote(limits.trace_root_move)<<"}\n";
        causal_trace_=std::make_unique<CausalTrace>(); causal_trace_->cap=limits.trace_max_events;
        causal_trace_->root_filter=limits.trace_root_move; causal_trace_->start=std::chrono::steady_clock::now();
        causal_trace_->min_iteration=limits.trace_min_iteration;
    }
#endif
    if(limits.safety_mask>SafetyAll) throw std::invalid_argument("Invalid safety mask");
    if(!limits.force_root_move.empty() && !b.parse_move(limits.force_root_move)) throw std::invalid_argument("Illegal forced root move");
    std::string policy=limits.features.names()+(limits.selective?"|selective":"|full")+(limits.pvs?"|pvs":"|ab")+(limits.use_tt?"|tt":"|no-tt");
    policy+="|correction-mask="+std::to_string(limits.correction_mask);
    policy+="|safety="+std::to_string(limits.safety_mask)+"|lmr="+std::to_string(limits.use_lmr)+"|null="+std::to_string(limits.use_null)+"|forced="+limits.force_root_move;
    policy+="|partial="+std::to_string(limits.partial_ordering);
#ifdef AXIOM_RESEARCH_TRACE
    policy+="|subtree="+std::to_string(limits.replay_subtree);
#endif
    if(policy!=previous_policy_) { clear(); previous_policy_=policy; }
    tt_->new_search();
    std::atomic_bool done=false; std::atomic<std::uint64_t> total=0;
    local_stop_=&done; shared_nodes_=&total; worker_id_=0;
    try { auto result=run_single(std::move(b),limits,stop,info); local_stop_=nullptr; shared_nodes_=nullptr;
#ifdef AXIOM_RESEARCH_TRACE
        if(hot_session.data) result.hot_profile=*hot_session.data;
        if(causal_trace_) causal_trace_->save(limits.trace_output);
#endif
        return result; }
    catch(...) { local_stop_=nullptr; shared_nodes_=nullptr;
#ifdef AXIOM_RESEARCH_TRACE
        if(causal_trace_) causal_trace_->save(limits.trace_output);
#endif
        throw; }
}
SearchResult Search::run_single(Board b,Limits limits,std::atomic_bool& stop,const std::function<void(const SearchResult&)>& info) {
    limits_=limits; limits_.depth=std::clamp(limits.depth,1,MaxPly-2); stop_=&stop; nodes_=0;
    b.legal_fast_path=limits.features.legal_fast_path;
    stats_=SearchStats{}; correction_samples_.clear(); lmr_samples_.clear(); trace_.fill(SearchTrace{}); pawn_cache_->hits=pawn_cache_->misses=0;
    start_=std::chrono::steady_clock::now(); deadline_=limits.milliseconds>0?start_+std::chrono::milliseconds(limits.milliseconds):std::chrono::steady_clock::time_point::max();
    if(limits.features.reuse_move_facts && !move_facts_) move_facts_=std::make_unique<MoveFacts[]>((MaxPly+1)*256);
    SearchResult result; evaluate_score(b,limits.features.strategic_eval,limits.features.king_safety,limits.features.pawn_cache?pawn_cache_.get():nullptr,&result.evaluation); result.features=limits.features.names();
    result.safety_mask=limits.safety_mask; result.force_root_move=limits.force_root_move;
    result.queue_ms=limits.queue_ms;
    std::vector<std::unique_ptr<Search>> helpers;
    std::vector<std::jthread> workers;
    std::vector<std::exception_ptr> failures(std::clamp(limits.threads,1,16));
    auto stop_workers=[&]() { if(worker_id_==0) { local_stop_->store(true); for(auto& worker:workers) if(worker.joinable()) worker.join(); } };
    struct JoinGuard { std::function<void()> fn; ~JoinGuard(){fn();} } join_guard{stop_workers};
    auto finish=[&]() {
        stop_workers();
        for(auto failure:failures) if(failure) std::rethrow_exception(failure);
        result.nodes=worker_id_==0?shared_nodes_->load():nodes_;
        result.stats=stats_; result.stats.threads=1+static_cast<int>(helpers.size());
        result.seldepth=stats_.seldepth;result.hashfull=tt_->hashfull();
        result.stats.pawn_cache_hits=pawn_cache_->hits; result.stats.pawn_cache_misses=pawn_cache_->misses;
        result.stats.root_correction=limits.features.correction?feedback_->correction(b,0,limits.correction_mask):0;
        result.correction_samples=correction_samples_;
        result.lmr_samples=lmr_samples_;
        // Aggregate event counts after joining; feedback tables remain private.
        for(const auto& helper:helpers) {
            result.seldepth=std::max(result.seldepth,helper->stats_.seldepth);
            result.stats.safety.add(helper->stats_.safety);
            for(unsigned q=0;q<MaxPly;++q) result.stats.qply_histogram[q]+=helper->stats_.qply_histogram[q];
#define AXIOM_SUM(field) result.stats.field += helper->stats_.field
            AXIOM_SUM(correction_updates); AXIOM_SUM(continuation_updates); AXIOM_SUM(capture_updates);
            AXIOM_SUM(tt_hits); AXIOM_SUM(null_attempts); AXIOM_SUM(null_verifications); AXIOM_SUM(null_rejections);
            AXIOM_SUM(singular_tests); AXIOM_SUM(singular_extensions); AXIOM_SUM(probcut_attempts); AXIOM_SUM(probcut_cutoffs);
            AXIOM_SUM(lmr_reductions); AXIOM_SUM(lmr_researches); AXIOM_SUM(history_prunes); AXIOM_SUM(see_prunes);
            AXIOM_SUM(mate_distance_cutoffs); AXIOM_SUM(iid_searches);
            AXIOM_SUM(safety_protected);
            AXIOM_SUM(qnodes); AXIOM_SUM(tt_cutoffs); AXIOM_SUM(null_cutoffs); AXIOM_SUM(pvs_scouts); AXIOM_SUM(pvs_researches);
            AXIOM_SUM(lmr_verifications_completed); AXIOM_SUM(horizon_interruptions); AXIOM_SUM(qcheck_evasions);
            AXIOM_SUM(rfp_cuts);
#define AXIOM_SUM_CAL(name) AXIOM_SUM(name);
            AXIOM_CALIBRATION_FIELDS(AXIOM_SUM_CAL)
#undef AXIOM_SUM_CAL
            result.stats.pawn_cache_hits+=helper->pawn_cache_->hits; result.stats.pawn_cache_misses+=helper->pawn_cache_->misses;
#undef AXIOM_SUM
        }
        result.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start_).count();
        result.hard_overshoot_ms=limits.milliseconds>0?std::max(0LL,result.elapsed_ms-limits.milliseconds):0;
        return result;
    };
    auto legal=b.legal_moves();
    if(legal.empty()) { result.status=b.in_check()?Status::ProvenMate:Status::ProvenForcedResult; result.outcome=b.in_check()?"LOSS":"DRAW"; result.score=b.in_check()?-MateScore:0; return finish(); }
    if(b.automatic_draw()) { result.status=Status::ProvenForcedResult; result.outcome="DRAW"; result.score=0; return finish(); }
    bool claim=b.can_claim_draw(true);
    if(!limits.force_root_move.empty()) legal.erase(std::remove_if(legal.begin(),legal.end(),[&](Move m){return m.uci()!=limits.force_root_move;}),legal.end());
    for(auto m:legal) {
        Applied applied(b,m); MoveResult r; r.move=m; r.score=-static_evaluation(b); r.pv={m}; result.moves.push_back(r);
        if(std::chrono::steady_clock::now()>=deadline_) break;
    }
    std::stable_sort(result.moves.begin(),result.moves.end(),[](const auto& a,const auto& c){return a.score>c.score;});
    result.best=result.moves.front().move; result.score=result.moves.front().score;
#ifdef AXIOM_RESEARCH_TRACE
    if(limits.replay_subtree) {
        try {
            std::vector<Move> line;
            const int score=negamax(b,limits_.depth,limits.replay_alpha,limits.replay_beta,0,limits.replay_beta-limits.replay_alpha>1,true,line);
            result.moves.clear(); result.best=line.empty()?Move{}:line.front(); result.score=score;
            result.depth=limits_.depth; result.status=Status::SearchResult;
            if(!line.empty()) { MoveResult move;move.move=line.front();move.pv=line;move.score=score;move.depth=result.depth;move.status=Status::SearchResult;
                move.bound=score<=limits.replay_alpha?Bound::Upper:score>=limits.replay_beta?Bound::Lower:Bound::Exact;result.moves.push_back(move); }
            result.verification_detail="Fresh-context direct negamax subtree replay; original TT/history/stack not restored";
        } catch(const Interrupted&) { result.verification_detail="Subtree replay interrupted; no completed score"; }
        return finish();
    }
#endif
    // Unrestricted proof/tablebase choices must not override a constrained replay.
    if(limits.force_root_move.empty()) result.tablebase=tablebase.probe(b);
    if(result.tablebase && result.tablebase->exact) {
        result.best=result.tablebase->best; result.status=Status::ExactTablebase;
        int w=result.tablebase->wdl; result.outcome=w==2?"WIN":w==-2?"LOSS":"DRAW"; result.score=w==2?28000:w==-2?-28000:0;
        for(auto& r:result.moves) if(r.move==result.best) { r.status=Status::ExactTablebase; r.score=result.score; r.tablebase=result.tablebase; r.bound=Bound::Exact; }
        return finish();
    }
    if(limits.force_root_move.empty() && limits.proof_nodes && limits.mate_plies>=1) {
        auto proof_deadline=limits.milliseconds>0?std::min(deadline_,start_+std::chrono::milliseconds(std::max(1,limits.milliseconds/5))):deadline_;
        auto budget=limits.nodes?std::min(limits.proof_nodes,limits.nodes/4):limits.proof_nodes;
        MateSolver solver; auto proof=solver.solve(b,b.side,limits.mate_plies,budget,&stop,proof_deadline);
        nodes_+=proof.nodes; shared_nodes_->fetch_add(proof.nodes); result.proof_nodes=proof.nodes;
        if(proof.proven && MateSolver::verify(b,b.side,proof.certificate)) {
            result.status=Status::ProvenMate; result.outcome="WIN"; result.certificate_verified=true;
            result.best=proof.certificate->edges.front().first; result.score=MateScore-proof.distance;
            for(auto& r:result.moves) if(r.move==result.best) { r.status=Status::ProvenMate; r.mate_distance=proof.distance; r.score=result.score; r.nodes=proof.nodes; r.bound=Bound::Lower;
                r.pv.clear(); auto node=proof.certificate;
                while(!node->edges.empty()) { auto edge=std::max_element(node->edges.begin(),node->edges.end(),[](const auto& a,const auto& c){return a.second->distance<c.second->distance;}); r.pv.push_back(edge->first); node=edge->second; }
            }
            result.verification_completed=true; result.verification_detail="Exhaustive defender coverage; certificate independently replayed"; return finish();
        }
    }
    if(worker_id_==0 && limits.threads>1 && !stop.load()) {
        for(int id=1;id<std::clamp(limits.threads,1,16);++id) {
            auto helper=std::make_unique<Search>(1); helper->tt_=tt_; helper->worker_id_=id;
            helper->local_stop_=local_stop_; helper->shared_nodes_=shared_nodes_;
            Search* ptr=helper.get(); helpers.push_back(std::move(helper));
            Limits worker_limits=limits; worker_limits.threads=1; worker_limits.proof_nodes=0; worker_limits.verify=false; worker_limits.diagnostics=false;
            worker_limits.soft_milliseconds=0; worker_limits.depth=std::min(MaxPly-2,limits.depth+id%2);
            workers.emplace_back([&,ptr,id,copy=b,worker_limits]() mutable {
                try { ptr->run_single(std::move(copy),worker_limits,stop,{}); }
                catch(...) { failures[id]=std::current_exception(); local_stop_->store(true); }
            });
        }
    }
    int stable_iterations=0; Move previous_best;
    result.setup_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start_).count();
    double previous_iteration_ms=0,estimate=0;
    try {
        for(int depth=1;depth<=limits_.depth;++depth) {
#ifdef AXIOM_RESEARCH_TRACE
            if(causal_trace_) causal_trace_->iteration_depth=depth;
#endif
            auto iteration_start=std::chrono::steady_clock::now();
            if(limits.time_guard && limits.milliseconds>0 && depth>1 && estimate+2>=std::chrono::duration<double,std::milli>(deadline_-iteration_start).count()) {
                result.time_guard_stopped=true; break;
            }
            const auto failures_before=stats_.aspiration_fail_low+stats_.aspiration_fail_high;
            const int previous_score=result.score;
            const auto previous_pv=result.moves.empty()?std::vector<Move>{}:result.moves.front().pv;
            int window=depth>=3 && std::abs(result.score)<MateThreshold?35+worker_id_*7:Infinity;
            std::vector<MoveResult> completed;
            for(;;) {
                int alpha=std::max(-Infinity,result.score-window),beta=std::min(Infinity,result.score+window);
                completed=root(b,depth,alpha,beta);
                int score=completed.front().score;
                if((score<=alpha || score>=beta) && window<2*Infinity) { if(score<=alpha) ++stats_.aspiration_fail_low; else ++stats_.aspiration_fail_high; window=std::min(2*Infinity,window*2); continue; }
                break;
            }
            result.moves=std::move(completed); result.best=result.moves.front().move; result.score=result.moves.front().score;
            result.depth=depth; result.status=Status::SearchResult;
#ifdef AXIOM_RESEARCH_TRACE
            if(causal_trace_) {
                for(std::size_t rank=0;rank<result.moves.size();++rank) {
                    CausalEvent e;e.reason="ROOT_ITERATION_RANK";e.fen=b.fen();e.depth=depth;e.ply=0;e.side=b.side;
                    e.move=result.moves[rank].move.uci();e.root_move=e.move;e.index=static_cast<int>(rank);e.score=result.moves[rank].score;
                    e.tt_bound=bound_name(result.moves[rank].bound);causal_trace_->emit(std::move(e),nodes_);
                }
            }
#endif
            double duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-iteration_start).count();
            if(estimate>0) result.iteration_estimate_error_ms=duration-estimate;
            estimate=next_iteration_cost(duration,previous_iteration_ms);
            previous_iteration_ms=duration; result.next_iteration_estimate_ms=estimate;
            for(const auto& move:result.moves) {
                auto found=std::find_if(result.root_history.begin(),result.root_history.end(),[&](const auto& h){return h.move==move.move;});
                if(found==result.root_history.end()) { RootMoveInfo h; h.move=move.move; result.root_history.push_back(h); found=std::prev(result.root_history.end()); }
                found->update(move);
            }
            std::uint64_t root_nodes=0; for(const auto& move:result.moves) root_nodes+=move.nodes;
            result.root_node_fraction=root_nodes?double(result.moves.front().nodes)/root_nodes:0;
            if(result.best==previous_best) ++stable_iterations;
            else { if(previous_best) ++stats_.bestmove_changes; stable_iterations=0; previous_best=result.best; }
            result.claim_draw=claim && result.score<=0; if(result.claim_draw) result.score=0;
            if(limits_.use_tt && limits_.force_root_move.empty()) tt_->store({b.hash(),b.proof_key(),depth,score_to_tt(result.score,0),Bound::Exact,result.best,true},limits.features.tt_policy);
            result.nodes=shared_nodes_->load(); result.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start_).count();
            if(info) {result.seldepth=stats_.seldepth;result.hashfull=tt_->hashfull();info(result);}
            if(worker_id_==0 && limits.features.time_management && limits.soft_milliseconds>0 && depth>=3) {
                double factor=stable_iterations>=3?0.7:stable_iterations==0?1.5:1.0;
                if(limits.features.adaptive_time) {
                    const int gap=result.moves.size()>1?result.score-result.moves[1].score:Infinity;
                    const bool pv_stable=previous_pv.size()>=2 && result.moves.front().pv.size()>=2 &&
                        previous_pv[0]==result.moves.front().pv[0] && previous_pv[1]==result.moves.front().pv[1];
                    const bool critical=stats_.aspiration_fail_low+stats_.aspiration_fail_high>failures_before ||
                        std::abs(result.score-previous_score)>60 || gap<30 || result.root_node_fraction>0.75;
                    factor=critical?1.5:(stable_iterations>=3 && pv_stable && gap>=80 && std::abs(result.score-previous_score)<=20?0.65:1.0);
                    if(legal.size()==1) factor=0.5;
                }
                result.time_factor=factor;
                if(result.elapsed_ms>=limits.soft_milliseconds*factor) { ++stats_.soft_time_stops; break; }
            }
        }
        if(limits.verify) {
            // Full-width root rerun with null-move, LMR and SEE/delta pruning
            // disabled. Still a finite search, not a game-theoretic proof.
            stop_workers(); local_stop_->store(false);
            bool strategic=limits_.features.strategic_eval,king_safety=limits_.features.king_safety;
            clear(); limits_.selective=false; limits_.features.all(false);
            limits_.features.strategic_eval=strategic; limits_.features.king_safety=king_safety; limits_.features.pawn_cache=true;
            int d=std::min(result.depth,4);
            previous_policy_="verification";
            auto verified=root(b,d,-Infinity,Infinity,true);
            for(auto& original:result.moves) for(const auto& checked:verified) if(original.move==checked.move) {
                original.tactical_verified=true; original.verification_depth=d; original.verification_score=checked.score;
                if(checked.pv.size()>1) original.strongest_reply=checked.pv[1];
            }
            for(auto& checked:verified) {
                checked.tactical_verified=true; checked.verification_depth=d; checked.verification_score=checked.score;
                if(checked.pv.size()>1) checked.strongest_reply=checked.pv[1];
            }
            result.verification_completed=true;
            result.verification_detail=verified.front().move==result.best?"Candidate agrees with nonselective verification search":"Alternative found by nonselective verification search";
            result.best=verified.front().move; result.score=verified.front().score; result.moves=std::move(verified); result.depth=d;
        }
    } catch(const Interrupted&) { if(limits.verify && !result.verification_completed) result.verification_detail="Incomplete: time, node limit or stop reached"; }
    if(limits.verify || limits.diagnostics) for(auto& r:result.moves) r.tactics=tactical_labels(b,r.move);
    result.claim_draw=claim && result.score<=0;
    if(result.claim_draw) result.score=0;
    return finish();
}

}
