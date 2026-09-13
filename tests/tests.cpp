#include "axiom/engine.hpp"
#include "axiom/retrograde.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <random>

using namespace axiom;
int checks=0;
void require(bool ok,const std::string& label) { ++checks; if(!ok) throw std::runtime_error(label); }
void play(Board& b,const std::string& s) { auto m=b.parse_move(s); require(m.has_value(),"legal "+s); b.push(*m); }
int reference_exchange(Board& b,int target) {
    int best=0;
    for(auto move:b.legal_moves()) if(move.to==target && b.capture(move)) {
        int gain=piece_value(b.squares[target])+(move.promotion?piece_value(move.promotion)-piece_value(Pawn):0);
        auto undo=b.push(move); int value=gain-reference_exchange(b,target); b.pop(undo);
        best=std::max(best,value);
    }
    return best;
}
int reference_see(Board& b,Move move) {
    int gain=b.capture(move)?piece_value(b.squares[move.to]?b.squares[move.to]:Pawn):0;
    if(move.promotion) gain+=piece_value(move.promotion)-piece_value(Pawn);
    auto undo=b.push(move); int value=gain-reference_exchange(b,move.to); b.pop(undo); return value;
}
int main() {
    try {
        Board b; auto fen=b.fen(); auto hash=b.hash();
        for(auto m:b.legal_moves()) { auto u=b.push(m); b.pop(u); require(b.fen()==fen && b.hash()==hash && b.history.size()==1,"make/unmake "+m.uci()); }
        require(perft(b,0)==1,"perft zero"); require(perft(b,1)==20,"start perft 1"); require(perft(b,2)==400,"start perft 2"); require(perft(b,3)==8902,"start perft 3"); require(perft(b,4)==197281,"start perft 4");
        Board kiwi("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        require(perft(kiwi,1)==48,"kiwipete 1"); require(perft(kiwi,2)==2039,"kiwipete 2"); require(perft(kiwi,3)==97862,"kiwipete 3");
        {
            PawnCache cache;
            for(Board position:{b,kiwi}) for(auto move:position.legal_moves()) {
                auto undo=position.push(move);
                int slow=evaluate(position).total,fast=evaluate_score(position,false,false,&cache);
                require(slow==fast,"fast evaluation preserves baseline score");
                require(evaluate_score(position,true,true,&cache)==evaluate_score(position,true,true,nullptr),"pawn cache preserves enhanced score");
                auto captures=position.legal_captures_to(move.to);
                for(auto capture:captures) require(position.parse_move(capture.uci()).has_value(),"targeted SEE captures remain legal");
                position.pop(undo);
            }
            auto before=cache.hits; evaluate_score(b,false,false,&cache); evaluate_score(b,false,false,&cache);
            require(cache.hits>before,"pawn cache hit recorded");
            for(int signal=0;signal<9;++signal) {
                SafetySignals safety; bool* fields[]={&safety.pv,&safety.tt,&safety.check,&safety.promotion,&safety.high_history,&safety.singular,&safety.strategic,&safety.improving,&safety.threat};
                *fields[signal]=true; require(safety.protect(),"every safety signal protects");
            }
            Board rookFile("6k1/ppp2ppp/8/8/8/8/PPP2PPP/R5K1 w - - 0 1");
            auto move=rookFile.parse_move("a1e1"); require(move && strategic_candidate(rookFile,*move,PawnCache::compute(rookFile)),"open file strategic candidate");
            Evaluation detail; evaluate_score(kiwi,true,true,&cache,&detail);
            require(detail.terms.contains("KingRingPressure") && detail.terms.contains("Outposts") && detail.terms.contains("PassedPotential"),"strategic debug terms exposed");
        }
        Board endgame("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        require(perft(endgame,4)==43238,"endgame EP perft 4");
        Board promotion("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        require(perft(promotion,3)==9467,"promotion/castling perft 3");
        Board ep("k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        require(!ep.parse_move("e5d6"),"EP exposing king is illegal");
        Board noep("k3r3/8/8/3pP3/8/8/8/4K3 w - - 0 1"); require(ep.hash()==noep.hash(),"unusable EP repetition identity");
        Board yesep("k7/8/8/3pP3/8/8/8/4K3 w - d6 0 1"), without("k7/8/8/3pP3/8/8/8/4K3 w - - 0 1");
        require(yesep.hash()!=without.hash(),"legal EP changes hash");
        auto epmove=yesep.parse_move("e5d6"); require(epmove.has_value(),"legal EP generated");
        require(see(yesep,*epmove)==100,"EP SEE");
        {
            std::mt19937 rng(20260913); Board sample; PawnCache cache;
            for(int ply=0;ply<200;++ply) {
                auto moves=sample.legal_moves();
                if(moves.empty() || sample.automatic_draw()) { sample=Board(); moves=sample.legal_moves(); }
                auto key=sample.proof_key(); auto history=sample.history; auto identity=sample.identities;
                require(evaluate(sample).total==evaluate_score(sample,false,false,&cache),"random fast/legacy evaluation equivalence");
                require(evaluate_score(sample,true,true,&cache)==evaluate_score(sample,true,true,nullptr),"random pawn-cache equivalence");
                for(auto move:moves) if(sample.capture(move) || move.promotion) require(see(sample,move)==reference_see(sample,move),"targeted SEE equals full legal recapture oracle");
                require(sample.proof_key()==key && sample.history==history && sample.identities==identity,"evaluation/SEE preserve complete history");
                sample.push(moves[rng()%moves.size()]);
            }
            Features preset; preset.middlegame();
            require(preset.correction && preset.continuation && preset.capture_history && preset.countermove && preset.dynamic_lmr && preset.king_safety && preset.strategic_eval && preset.search_safety && preset.pawn_cache,"middlegame preset enables priorities");
            require(!preset.probcut && !preset.see_pruning && !preset.history_pruning && !preset.singular,"middlegame preset keeps aggressive experiments separate");
        }
        Board castle("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"); play(castle,"e1g1"); require(castle.squares[5]==Rook && castle.squares[6]==King && !(castle.castle&(WK|WQ)),"castling rook and rights");
        Board rep;
        for(int n=0;n<2;++n) for(auto m:{"g1f3","g8f6","f3g1","f6g8"}) play(rep,m);
        require(rep.repetitions()==3 && rep.can_claim_draw() && !rep.automatic_draw(),"threefold is optional");
        for(int n=0;n<2;++n) for(auto m:{"g1f3","g8f6","f3g1","f6g8"}) play(rep,m);
        require(rep.automatic_draw(),"fivefold automatic");
        Board collision;
        collision.history={collision.hash(),collision.hash(),collision.hash()};
        collision.identities={std::string(67,'x'),std::string(67,'y'),collision.identities.front()};
        collision.halfmove=8;
        require(collision.repetitions()==1,"hash collision cannot manufacture repetition");
        Board fifty("7k/8/8/8/8/8/R7/K7 w - - 99 1"); require(fifty.can_claim_draw(),"claim by intended 100th halfmove");
        Board mate("7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
        MateSolver solver; auto proof=solver.solve(mate,White,3,100000);
        require(proof.proven && proof.distance==1 && MateSolver::verify(mate,White,proof.certificate),"mate in one certified");
        auto bad=std::make_shared<ProofNode>(*proof.certificate); bad->distance+=2; require(!MateSolver::verify(mate,White,bad),"tampered certificate rejected");
        Board mate2("7k/8/8/5K2/5Q2/8/8/8 w - - 0 1");
        auto p2=solver.solve(mate2,White,3,100000); require(p2.proven && p2.distance==3 && MateSolver::verify(mate2,White,p2.certificate),"mate in two all defenses verified");
        auto incomplete=std::make_shared<ProofNode>(*p2.certificate);
        incomplete->edges[0].second=std::make_shared<ProofNode>(*incomplete->edges[0].second);
        incomplete->edges[0].second->edges.pop_back();
        require(!MateSolver::verify(mate2,White,incomplete),"missing defender edge rejected");
        Board claimDefense("7k/8/8/5K2/5Q2/8/8/8 w - - 98 1");
        require(!solver.solve(claimDefense,White,3,100000).proven,"defender draw claim prevents mate proof");
        auto unknown=solver.solve(b,White,1,1); require(!unknown.proven && unknown.exhausted,"budget exhaustion is unknown");
        Board stalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"); require(stalemate.legal_moves().empty() && !stalemate.in_check(),"stalemate");
        require(!solver.solve(stalemate,White,3,10000).proven,"stalemate not mate");
        Board dead("7k/8/8/8/8/8/8/K7 w - - 0 1"); require(dead.insufficient(),"bare kings");
        Board twoKnights("7k/8/8/8/8/8/NN6/K7 w - - 0 1"); require(!twoKnights.insufficient(),"KNN potential mate must not be dead-labelled");
        auto retro=retrograde({{}, {0}, {1}, {3}, {0,3}}, {-1,2,2,2,2});
        require(retro[1].result==1 && retro[1].distance==1 && retro[2].result==-1 && retro[2].distance==2 && retro[3].result==0 && retro[4].result==1,"retrograde propagation and cycles");
        Search engine(1); std::atomic_bool stop=false; Limits limits; limits.depth=2; limits.milliseconds=0; limits.proof_nodes=0;
        auto result=engine.run(b,limits,stop); require(result.depth==2 && b.parse_move(result.best.uci()) && result.status==Status::SearchResult,"search legal bestmove");
        require(result.moves.size()==20,"root result for every legal move");
        for(const auto& r:result.moves) { Board copy=b; for(auto m:r.pv) { require(copy.parse_move(m.uci()).has_value(),"legal PV"); copy.push(m); } }
        limits.proof_nodes=10000; limits.mate_plies=3; auto solved=engine.run(mate,limits,stop); require(solved.status==Status::ProvenMate && solved.certificate_verified,"engine exact mate priority");
        auto drawn=engine.run(stalemate,limits,stop); require(drawn.outcome=="DRAW" && !drawn.best,"terminal no bestmove");
        limits.proof_nodes=0; limits.nodes=1; auto interrupted=engine.run(b,limits,stop); require(b.parse_move(interrupted.best.uci()).has_value() && interrupted.nodes<=1,"node budget legal fallback");
        limits.nodes=0; limits.verify=true; auto verified=engine.run(b,limits,stop); require(verified.verification_completed && verified.status==Status::SearchResult,"verification remains finite search");
        for(const auto& r:verified.moves) require(r.tactical_verified && r.verification_depth==2 && bool(r.strongest_reply),"every candidate verified with strongest reply");
        require(!engine.tablebase.enabled(),"missing tablebase no fabricated exactness");
        {
            Features features; require(!features.correction && !features.strategic_eval && features.pawn_cache,"unmeasured features off, exact pawn cache on");
            require(features.set("correction",true) && features.correction && !features.set("typo",true),"feature switches validated");
            Feedback feedback; std::int16_t bounded=0;
            for(int i=0;i<10000;++i) Feedback::update(bounded,1500);
            require(bounded>0 && bounded<=16000,"history positive saturation");
            for(int i=0;i<10000;++i) Feedback::update(bounded,-1500);
            require(bounded<0 && bounded>=-16000,"history negative saturation");
            for(int i=0;i<500;++i) feedback.update_correction(b,0,1000,12);
            require(feedback.correction(b,0)>0 && feedback.correction(b,0)<=256,"correction direction and clamp");
            int previous=Feedback::token(Knight,33),current=Feedback::token(-Pawn,67);
            for(int i=0;i<4;++i) { feedback.update_continuation(previous,current,i,100); require(feedback.continuation(previous,current,i)>0,"continuation distance table"); }
            feedback.update_capture(Knight,67,Bishop,100); require(feedback.capture_score(Knight,67,Bishop)>0,"capture history key");
            Move counter{99,67,0}; feedback.set_counter(previous,counter); require(feedback.counter(previous)==counter,"countermove retrieval");
            feedback.clear(); require(feedback.correction(b,0)==0 && feedback.capture_score(Knight,67,Bishop)==0 && !feedback.counter(previous),"feedback reset");
            for(int i=0;i<500;++i) feedback.update_correction(b,0,80,8);
            require(feedback.correction(b,0)>=79 && feedback.correction(b,0)<=80,"correction converges to observed error");
            Board other=b; other.side=Black;
            require(feedback.correction(other,0)==0,"correction side-to-move isolation");
        }
        {
            TranspositionTable table(0); table.new_search();
            table.store({1,"deep",12,90,Bound::Exact,{1,33,0},true},true);
            table.store({1,"deep",1,5,Bound::Lower,{1,18,0},false},true);
            require(table.probe(1,"deep")->depth==12,"deep exact PV preserved");
            require(!table.probe(1,"different history"),"TT repetition context separated");
            for(int key=2;key<=5;++key) table.store({std::uint64_t(key),std::to_string(key),1,3,Bound::Upper,{},false},true);
            require(table.probe(1,"deep").has_value(),"cluster replacement protects deep exact");
            for(int age=0;age<10;++age) table.new_search();
            table.store({1,"deep",1,3,Bound::Upper,{},false},true);
            require(table.probe(1,"deep")->depth==1,"age permits refresh");
            std::atomic_bool torn=false; std::vector<std::jthread> writers;
            for(int thread=0;thread<4;++thread) writers.emplace_back([&,thread] {
                for(int i=0;i<1000;++i) {
                    int depth=1+(i+thread)%12; table.store({11,"shared",depth,depth*3,Bound::Exact,{},false},true);
                    auto entry=table.probe(11,"shared"); if(entry && entry->score!=entry->depth*3) torn=true;
                }
            });
            for(auto& writer:writers) writer.join(); require(!torn,"shared TT snapshots are consistent");
        }
        {
            auto experimental=std::make_unique<Search>(1); Limits opts; opts.depth=4; opts.milliseconds=0; opts.proof_nodes=0; opts.nodes=20000; opts.features.all(true);
            auto feedbackResult=experimental->run(b,opts,stop);
            require(b.parse_move(feedbackResult.best.uci()).has_value() && feedbackResult.status==Status::SearchResult,"experimental legal unproven result");
            require(feedbackResult.stats.correction_updates>0 && feedbackResult.stats.continuation_updates>0,"search exercises feedback");
            require(feedbackResult.stats.safety_protected>0 && feedbackResult.stats.pawn_cache_hits>0,"safety and pawn cache exercised");
            opts.threads=3; opts.depth=8; opts.nodes=2500;
            auto parallel=experimental->run(b,opts,stop);
            require(parallel.stats.threads==3 && parallel.nodes<=2500 && b.parse_move(parallel.best.uci()).has_value(),"SMP shared node cap and legal fallback");
            opts.threads=1; opts.nodes=0; opts.depth=2; opts.verify=true;
            auto check=experimental->run(b,opts,stop);
            require(check.verification_completed && check.status==Status::SearchResult,"experimental verification disables heuristics");
            opts.verify=false; opts.proof_nodes=10000; opts.mate_plies=3;
            auto certified=experimental->run(mate,opts,stop);
            require(certified.status==Status::ProvenMate && certified.certificate_verified && certified.stats.correction_updates==0,"feedback cannot override certificate");
        }
        std::cout<<"PASS "<<checks<<" checks\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n'; return 1; }
}
