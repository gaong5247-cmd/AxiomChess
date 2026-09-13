#include "axiom/engine.hpp"
#include "axiom/san.hpp"
#include <iostream>
#include <stdexcept>
using namespace axiom;
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        check(parse_safety_mask("legacy")==SafetyLegacy && parse_safety_mask("all")==SafetyAll,"safety defaults");
        check(next_iteration_cost(10,5)==20 && next_iteration_cost(1,100)==1.5 && next_iteration_cost(10,0)==30,"bounded iteration estimator");
        check(parse_safety_mask("no-king")== (SafetyAll & ~SafetyKing),"named safety ablation");
        for(const auto& invalid:{"-1","16384","all,no-king","2junk"}) {
            bool rejected=false; try { parse_safety_mask(invalid); } catch(const std::exception&) { rejected=true; }
            check(rejected,"invalid safety mask rejected");
        }
        check(eval_trend(200,100)==100 && eval_trend(-200,100)==-256,"bounded worsening trend");
        check(eval_trend(Infinity,100)==0 && eval_trend(100,Infinity)==0,"check/missing stack sentinel");
        check(clock_available(100,true)==25 && clock_available(50,true)==1 && clock_available(2000,true)==1900,"adaptive clock reserve");
        auto feedback=std::make_unique<Feedback>(); Board b;
        for(int i=0;i<10000;++i) feedback->update_correction(b,0,100000,20);
        for(auto v:feedback->correction_components(b,0)) check(v>=-4096 && v<=4096,"bounded correction component");
        check(feedback->correction(b,0,0)==0,"zero component mask");
        auto s=std::make_unique<Search>(1); std::atomic_bool stop=false;
        Limits l; l.depth=4; l.milliseconds=0; l.proof_nodes=0; l.features.middlegame(); l.calibration_samples=128;
        auto r=s->run(b,l,stop);
        {
            auto cached=std::make_unique<Search>(1);
            auto cache_limits=l; cache_limits.features.reuse_move_facts=true;
            auto cr=cached->run(b,cache_limits,stop);
            check(cr.best==r.best && cr.score==r.score && cr.depth==r.depth && cr.nodes==r.nodes,
                  "cached move facts preserve fresh search result and tree");
            check(!l.features.reuse_move_facts,"move fact reuse remains experimental OFF");
        }
        const auto frozen=s->correction_state();
        const auto snapshot=s->frozen_correction(b);
        check(frozen==s->correction_state() && snapshot.components==s->frozen_correction(b).components,"frozen correction is read only");
        check(!l.features.rfp && !l.features.calibrated_lmr && !l.features.adaptive_time,"rejected experiments stay off");
        {
            auto forced=l; forced.force_root_move="a2a3"; forced.use_lmr=false; forced.use_null=false;
            auto replay=s->run(b,forced,stop);
            check(replay.best.uci()=="a2a3" && replay.moves.size()==1 && replay.depth==4,"forced root complete search");
            check(replay.stats.lmr_reductions==0 && replay.stats.null_attempts==0,"actual LMR/null ablation");
            forced.force_root_move="a2a5"; bool rejected=false;
            try { s->run(b,forced,stop); } catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"illegal forced root rejected");
            forced.force_root_move="a2a3"; forced.milliseconds=1; forced.depth=30;
            check(s->run(b,forced,stop).best.uci()=="a2a3","forced root timed fallback");
        }
        check(!r.correction_samples.empty(),"pre-update quality samples connected");
        for(auto sample:r.correction_samples) check(std::abs(sample.score)<MateThreshold,"mate label excluded");
        l.features.trend_safety=true; auto trend=s->run(b,l,stop);
        check(b.parse_move(trend.best.uci()).has_value(),"trend safety legal result");
        check(!trend.root_history.empty() && trend.root_history.front().observations>0,"root iteration history");
        check(parse_san(b,"Nf3").uci()=="g1f3","SAN conversion");
        bool rejected=false; try { parse_san(b,"Qh9"); } catch(const std::exception&) { rejected=true; }
        check(rejected,"invalid SAN rejected");
        Board promotion("1r5k/P7/8/8/8/8/8/7K w - - 0 1");
        check(parse_san(promotion,"axb8=Q+").uci()=="a7b8q","SAN promotion capture");
        Board castling("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        check(parse_san(castling,"O-O").uci()=="e1g1","SAN castling");
        auto terminal=s->run(Board("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"),l,stop);
        check(terminal.stats.correction_updates==0 && terminal.correction_samples.empty(),"proven mate excluded from correction learning");
        Board quiet("7k/8/8/4K3/4Q3/8/8/8 w - - 0 1");
        l.depth=5; l.features.calibrated_lmr=true; l.features.history_pruning=true;
        auto mate=s->run(quiet,l,stop); check(mate.score==MateScore-5,"quiet mating continuation preserved");
        l.depth=30; l.milliseconds=60; l.soft_milliseconds=15;
        l.features.time_management=true; l.features.adaptive_time=true;
        auto timed=s->run(b,l,stop);
        check(timed.elapsed_ms<260 && b.parse_move(timed.best.uci()).has_value(),"deadline with scheduling tolerance");
        l.time_guard=true;
        auto guarded=s->run(b,l,stop);
        check(guarded.elapsed_ms<260 && b.parse_move(guarded.best.uci()).has_value(),"experimental iteration guard deadline");
        std::cout<<"PASS calibration: bounded corrections, component mask, mate exclusion, trend stack\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
