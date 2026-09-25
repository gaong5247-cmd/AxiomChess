#include "search.hpp"
#include "tablebase.hpp"
#include <thread>
namespace ax2 {
Result Engine::run(const Position& p,const Limits& l,const Features& f,int threads,int multiPV,const Reporter& report,Tablebase* tb){
    if(tb&&!l.restrictMoves&&multiPV==1&&p.halfmove==0){if(auto probe=tb->root(p)){Result r;r.best=probe->move;r.score=probe->score;r.pv.moves[0]=r.best;r.pv.size=1;r.stats.tbHits=1;return r;}}
    std::vector<std::jthread> helpers;std::vector<std::unique_ptr<Worker>> workers;
    std::exception_ptr failure;std::mutex failureMutex;
    struct Join {Control& c;std::vector<std::jthread>& threads;~Join(){c.stop=true;for(auto& t:threads)if(t.joinable())t.join();}} join{control,helpers};
    for(int id=1;id<std::clamp(threads,1,64);++id){workers.push_back(std::make_unique<Worker>(tt,control));auto* worker=workers.back().get();worker->tb=tb;
        helpers.emplace_back([&,worker,id]{try{worker->run(p,l,f,id,1,{});}catch(...){std::lock_guard guard(failureMutex);failure=std::current_exception();control.stop=true;}});}
    auto main=std::make_unique<Worker>(tt,control);main->tb=tb;auto result=main->run(p,l,f,0,multiPV,report);
    control.stop=true;for(auto& helper:helpers)helper.join();
    if(failure)std::rethrow_exception(failure);
    result.nodes=control.nodes.load();result.ms=control.time.elapsed();return result;
}
}
