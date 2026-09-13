#include "axiom/engine.hpp"
#include "search_internal.hpp"
#include <algorithm>
namespace axiom {
using search_detail::Applied;
using search_detail::Interrupted;
int Search::quiescence(Board& b,int alpha,int beta,int ply,int qply,bool synthetic,bool clean) {
    AXIOM_HOT(QSearch,QSearch);
    AXIOM_TRACE_SCOPE(causal_trace_.get(),b,0,alpha,beta,ply,qply,trace_[0].move.uci(),beta-alpha>1,synthetic,clean,nodes_);
    AXIOM_TRACE_EVENT("QSEARCH_ENTER",Move{},std::nullopt);
    ProfileScope timer(limits_.profile,stats_.qsearch_ns);
    const bool wide_window=beta-alpha>1;
    tick(); ++stats_.qnodes;
    stats_.seldepth=std::max(stats_.seldepth,ply);
    auto moves=[&] { ProfileScope timer(limits_.profile,stats_.legal_ns); return b.legal_moves(); }(); bool check=b.in_check();
    ++stats_.qply_histogram[std::min(qply,MaxPly-1)];
    stats_.q_generated+=moves.size(); if(check) ++stats_.q_checked_nodes;
    if(moves.empty()) return check?-MateScore+ply:0;
    if(synthetic?b.insufficient():b.automatic_draw()) return 0;
    bool claim=!synthetic && b.can_claim_draw(true);
    int raw=static_evaluation(b); int stand=!synthetic && !clean?corrected_eval(b,raw,ply):raw;
    AXIOM_TRACE_DETAIL(causal.base.raw=raw; causal.base.corrected=stand; causal.base.correction=feedback_->correction_components(b,previous_token(ply)));
    trace_[ply].static_eval=check?Infinity:stand;
    if(ply>=MaxPly-1 && check) { ++stats_.horizon_interruptions; throw Interrupted{}; }
    if(ply>=MaxPly-1 || (qply>=20 && !check)) { AXIOM_TRACE_EVENT("QSEARCH_HORIZON",Move{},stand); return claim?std::max(0,stand):stand; }
    int best=check?-Infinity:stand;
    if(claim) best=std::max(best,0);
    if(claim) { if(beta<=0) return 0; alpha=std::max(alpha,0); }
    if(!check) { if(stand>=beta) { ++stats_.q_stand_cutoffs; AXIOM_TRACE_EVENT("QSEARCH_STAND_CUTOFF",Move{},stand); return stand; } alpha=std::max(alpha,stand); }
    const bool safety=limits_.features.search_safety;
    const bool improving=safety && !check && ply>=2 && trace_[ply-2].static_eval!=Infinity && stand>trace_[ply-2].static_eval;
    // Keep terminal detection above, but never sort quiet moves that qsearch
    // cannot visit. Filtering preserves the relative order of retained moves.
    if(!check) moves.erase(std::remove_if(moves.begin(),moves.end(),[&](Move m) {
        return !b.capture(m) && !m.promotion && !(qply<3 && b.gives_check(m));
    }),moves.end());
    order(b,moves,{},ply);
    for(std::size_t ordered_index=0;auto m:moves) {
        const MoveFacts* facts=limits_.features.reuse_move_facts && move_facts_ && moves.size()<=256?&move_facts_[ply*256+ordered_index]:nullptr;
        ++ordered_index;
        bool cap=b.capture(m), checking=facts?facts->checking:b.gives_check(m);
        if(!check && !cap && !m.promotion && !(checking && qply<3)) continue;
        unsigned reasons=0;
        if(safety && cap) reasons=(wide_window?SafetyPV:0u)|(improving?SafetyImproving:0u)|
            (history_score(b,m,ply)>2000?SafetyHistory:0u)|
            ((b.attacked(m.from,-b.side) && !b.attacked(m.from,b.side))?SafetyTactical:0u);
        reasons=safety?reasons & limits_.safety_mask:0;
        bool protected_capture=cap && reasons;
        AXIOM_TRACE_DETAIL(causal.base.alpha=alpha; causal.base.safety=reasons; causal.base.capture=cap; causal.base.check=checking; causal.base.promotion=bool(m.promotion));
        if(limits_.selective && !clean && !check && cap && !checking && !m.promotion && protected_capture) {
            stats_.safety.record(SafetyAction::Futility,reasons);
            stats_.safety.record(SafetyAction::See,reasons);
            AXIOM_TRACE_DETAIL(if(stand+piece_value(b.squares[m.to])+250<alpha && std::abs(b.squares[m.from])!=Pawn) causal.event("QDELTA_BLOCKED_BY_SAFETY",nodes_,m);
                causal.base.see=see(b,m); if(*causal.base.see<0) causal.event("QSEE_BLOCKED_BY_SAFETY",nodes_,m));
        }
        if(limits_.selective && !clean && !check && cap && !checking && !m.promotion && !protected_capture) {
            if(stand+piece_value(b.squares[m.to])+250<alpha && std::abs(b.squares[m.from])!=Pawn) { ++stats_.q_delta_prunes; AXIOM_TRACE_EVENT("QSEARCH_DELTA_PRUNE",m,std::nullopt); continue; }
            int exchange=facts && facts->see_known?facts->exchange:see(b,m); AXIOM_TRACE_DETAIL(causal.base.see=exchange);
            if(exchange<0) { ++stats_.q_see_prunes; AXIOM_TRACE_EVENT("QSEARCH_SEE_PRUNE",m,std::nullopt); continue; }
        }
        trace_[ply].move=m; trace_[ply].piece=b.squares[m.from];
        if(check) ++stats_.qcheck_evasions;
        ++stats_.q_searched; stats_.q_captures+=cap; stats_.q_promotions+=bool(m.promotion); stats_.q_checks+=checking;
        AXIOM_TRACE_EVENT("QSEARCH_MOVE_BEGIN",m,std::nullopt);
        int score; { Applied applied(b,m); score=-quiescence(b,-beta,-alpha,ply+1,qply+1,synthetic,clean); }
        AXIOM_TRACE_EVENT("QSEARCH_MOVE_RESULT",m,score);
        best=std::max(best,score);
        if(score>=beta) { ++stats_.q_beta_cutoffs; AXIOM_TRACE_EVENT("QSEARCH_BETA_CUTOFF",m,score); return score; } alpha=std::max(alpha,score);
    } return best;
}
}
