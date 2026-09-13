#include "axiom/engine.hpp"
#include <iostream>
#include <stdexcept>
#include <random>
#include <algorithm>
using namespace axiom;
namespace axiom {
struct SearchAudit {
    static int node(Search& s,Board b,int depth,int alpha,int beta,std::optional<TTEntry> entry={},bool selective=false,Features features={},bool auxiliary=false) {
        static std::atomic_bool stop=false;
        s.clear(); s.stop_=&stop; s.limits_=Limits{}; s.limits_.selective=selective; s.limits_.features=features;
        s.deadline_=std::chrono::steady_clock::time_point::max(); s.nodes_=0; s.stats_={};
        if(entry) { entry->key=b.hash(); entry->context=b.proof_key(); entry->move=b.legal_moves().front(); s.tt_->store(*entry,false); }
        std::vector<Move> line;
        return s.negamax(b,depth,alpha,beta,2,false,true,line,false,{},auxiliary);
    }
    static SearchStats stats(const Search& s) { return s.stats_; }
    static int q(Search& s,Board b,int qply,int ply=0,bool synthetic=false) {
        static std::atomic_bool stop=false;
        s.clear(); s.stop_=&stop; s.limits_=Limits{}; s.limits_.selective=false;
        s.deadline_=std::chrono::steady_clock::time_point::max(); s.nodes_=0;
        return s.quiescence(b,-Infinity,Infinity,ply,qply,synthetic);
    }
};
}
void require(bool value,const char* label) { if(!value) throw std::runtime_error(label); }
void legal_pvs(Board b,const SearchResult& r) {
    if(r.best) require(b.parse_move(r.best.uci()).has_value(),"illegal root move");
    for(const auto& row:r.moves) { Board position=b;
        for(auto m:row.pv) { require(position.parse_move(m.uci()).has_value(),"illegal PV move"); position.push(m); }
    }
}
int main() {
    try {
        auto s=std::make_unique<Search>(1);
        Board check("7k/8/8/8/8/8/4r3/4KQ2 w - - 0 1");
        int raw=evaluate_score(check),q=SearchAudit::q(*s,check,20);
        if(q<raw+300) throw std::runtime_error("qply limit uses static evaluation in check instead of capturing checking rook");
        bool stopped=false; try { SearchAudit::q(*s,check,0,MaxPly-1); } catch(...) { stopped=true; }
        if(!stopped) throw std::runtime_error("hard ply cap must not return stand-pat in check");
        if(SearchAudit::q(*s,Board("7k/8/8/8/8/8/8/KN6 w - - 0 1"),0,0,true)!=0)
            throw std::runtime_error("synthetic null subtree must preserve insufficient-material draws");
        for(int score:{-MateScore+8,MateScore-8,-120,0,200}) for(int ply:{0,1,7,19}) {
            if(score_from_tt(score_to_tt(score,ply),ply)!=score) throw std::runtime_error("TT mate round trip");
        }
        if(score_from_tt(score_to_tt(MateScore-12,7),2)!=MateScore-7 ||
           score_from_tt(score_to_tt(-MateScore+12,7),2)!=-MateScore+7)
            throw std::runtime_error("TT mate normalization across different plies");
        std::atomic_bool stop=false; Limits limits; limits.depth=5; limits.milliseconds=0;
        limits.proof_nodes=0; limits.verify=true; limits.features.middlegame();
        auto verified=s->run(Board(),limits,stop);
        if(!verified.verification_completed || verified.depth!=4 || verified.score!=verified.moves.front().verification_score)
            throw std::runtime_error("verification must publish consistent score/depth even when best move agrees");
        for(auto bound:{Bound::Exact,Bound::Lower,Bound::Upper}) {
            int score=bound==Bound::Upper?-51:51;
            TTEntry entry; entry.depth=3; entry.score=score; entry.bound=bound;
            int actual=SearchAudit::node(*s,Board(),3,-50,50,entry);
            require(actual==score && SearchAudit::stats(*s).tt_cutoffs==1,"TT exact/lower/upper cutoff semantics");
        }
        TTEntry shallow; shallow.depth=1; shallow.score=12345; shallow.bound=Bound::Exact;
        require(SearchAudit::node(*s,Board(),2,-50,50,shallow)!=12345,"TT shallow score must not cut deep search");
        TTEntry wrongBound; wrongBound.depth=3; wrongBound.score=51; wrongBound.bound=Bound::Upper;
        require(SearchAudit::node(*s,Board(),2,-50,50,wrongBound)!=51,"TT upper score above alpha is not a lower cutoff");
        Limits exact; exact.depth=3; exact.proof_nodes=0; exact.milliseconds=0; exact.selective=false; exact.use_tt=false;
        Board sample; std::mt19937 rng(19);
        for(int n=0;n<12;++n) {
            s->clear(); exact.pvs=true; auto pvs=s->run(sample,exact,stop);
            s->clear(); exact.pvs=false; auto full=s->run(sample,exact,stop);
            require(pvs.depth==3 && full.depth==3 && pvs.score==full.score,"PVS differs from full-window alpha-beta");
            legal_pvs(sample,pvs); legal_pvs(sample,full);
            auto moves=sample.legal_moves(); sample.push(moves[rng()%moves.size()]);
        }
        Limits tactical; tactical.depth=5; tactical.milliseconds=0; tactical.proof_nodes=0; tactical.features.middlegame();
        Board m3("7k/8/8/4K3/4Q3/8/8/8 w - - 0 1");
        auto mate3=s->run(m3,tactical,stop); legal_pvs(m3,mate3);
        require(mate3.score==MateScore-5 && !m3.gives_check(mate3.best),"quiet tactical mate in three");
        MateSolver solver; auto certificate=solver.solve(m3,White,5,100000);
        require(certificate.proven && certificate.distance==5 && MateSolver::verify(m3,White,certificate.certificate),"mate in three independent certificate");
        Board under("8/k1P5/2K5/8/8/8/8/8 w - - 0 1");
        tactical.depth=4; auto underResult=s->run(under,tactical,stop); legal_pvs(under,underResult);
        require(underResult.best.uci()=="c7c8r" && underResult.score>MateThreshold,"rook underpromotion avoids stalemate");
        Board only("7r/8/8/8/8/5k2/8/7K w - - 0 1");
        auto defense=s->run(only,tactical,stop); require(defense.best.uci()=="h1g1","only move defense"); legal_pvs(only,defense);
        Board poison("r6k/8/8/8/p7/8/8/3Q3K w - - 0 1");
        require(see(poison,*poison.parse_move("d1a4"))==-850,"poisoned pawn SEE");
        tactical.depth=3; auto avoid=s->run(poison,tactical,stop); require(avoid.best.uci()!="d1a4","avoid poisoned capture"); legal_pvs(poison,avoid);
        Board xray("r6k/8/r7/8/p7/8/R7/R6K w - - 0 1");
        require(see(xray,*xray.parse_move("a2a4"))==-400,"x-ray SEE recapture chain");
        Board pinned("4k3/3pr3/2B5/8/8/8/8/3RR1K1 w - - 0 1");
        require(see(pinned,*pinned.parse_move("c6d7"))==100,"pinned rook and illegal king recapture excluded from SEE");
        Board promote("1r5k/P7/8/8/8/8/8/7K w - - 0 1");
        require(see(promote,*promote.parse_move("a7b8q"))==1350,"promotion capture SEE gain");
        Board discovered("7k/8/8/8/3N4/8/1B6/6K1 w - - 0 1");
        auto labels=tactical_labels(discovered,*discovered.parse_move("d4f5"));
        require(std::find(labels.begin(),labels.end(),"discovered_check")!=labels.end(),"discovered attack recognized");
        Board zwischen("7k/8/6K1/8/2B5/8/4r3/4Q1R1 w - - 0 1");
        auto intermezzo=s->run(zwischen,tactical,stop); legal_pvs(zwischen,intermezzo);
        require(intermezzo.score==MateScore-1 && intermezzo.best.uci()!="e1e2","checking intermezzo before recapture");
        Board zug("8/4k3/8/4K3/4P3/8/8/8 w - - 0 1");
        tactical.features.all(true); auto ending=s->run(zug,tactical,stop);
        require(ending.stats.null_attempts==0,"pawn ending must not use null move"); legal_pvs(zug,ending);
        Board repetition("7k/8/8/8/8/8/6q1/K7 w - - 0 1");
        for(int n=0;n<2;++n) for(auto move:{"a1b1","h8g8","b1a1","g8h8"}) repetition.push(*repetition.parse_move(move));
        auto draw=s->run(repetition,tactical,stop); require(draw.claim_draw && draw.score==0,"threefold claim selected in lost position");
        Board fifty("7k/8/8/8/8/8/6q1/K7 w - - 99 1");
        auto fiftyResult=s->run(fifty,tactical,stop); require(fiftyResult.claim_draw && fiftyResult.score==0,"intended 50 move claim selected");
        auto terminal=s->run(Board("7k/6Q1/6K1/8/8/8/8/8 b - - 150 1"),tactical,stop);
        require(terminal.status==Status::ProvenMate && terminal.score==-MateScore,"mate precedes 75 move draw");
        Board rights("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        auto original=rights.hash(); rights.castle=0; require(rights.hash()!=original,"castling hash component");
        original=rights.hash(); rights.side=-rights.side; require(rights.hash()!=original,"side hash component");
        tactical.depth=8; tactical.features.all(false); tactical.features.dynamic_lmr=true; tactical.features.pawn_cache=true;
        auto reduced=s->run(Board("r3k2r/ppp2ppp/8/8/8/8/PPP2PPP/R2QK2R w KQkq - 0 1"),tactical,stop);
        require(reduced.stats.lmr_researches>0 && reduced.stats.lmr_verifications_completed==reduced.stats.lmr_researches,"LMR fail-highs complete full-depth verification");
        Features rfp; rfp.rfp=true;
        SearchAudit::node(*s,Board(),2,-1001,-1000,{},true,rfp);
        require(SearchAudit::stats(*s).rfp_cuts==1,"RFP connected to quiet stable non-PV node");
        SearchAudit::node(*s,zug,2,-1001,-1000,{},true,rfp);
        require(SearchAudit::stats(*s).rfp_cuts==0,"RFP forbidden in pawn ending");
        SearchAudit::node(*s,check,1,-1001,-1000,{},true,rfp);
        require(SearchAudit::stats(*s).rfp_cuts==0,"RFP forbidden in check");
        Features auxiliary; auxiliary.all(true);
        SearchAudit::node(*s,Board(),3,-Infinity,Infinity,{},true,auxiliary,true);
        auto isolated=SearchAudit::stats(*s);
        require(isolated.correction_updates==0 && isolated.continuation_updates==0 && isolated.capture_updates==0 && isolated.tt_probes==0,"IID/excluded auxiliary search must not train histories or probe normal TT");
        std::cout<<"PASS audit: horizons, verification, TT bounds/mates, 12 PVS comparisons, legal PVs, tactics/draws, LMR re-search\n";
    } catch(const std::exception& e) { std::cerr<<"FAIL audit: "<<e.what()<<'\n'; return 1; }
}
