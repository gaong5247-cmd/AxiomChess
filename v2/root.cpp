#include "search.hpp"
#include <cmath>
namespace ax2 {
int Worker::root_search(Position& p,std::vector<RootMove>& roots,int depth,int alpha,int beta,const std::vector<Move>& excluded){
    int best=-Inf,searched=0;
    for(auto& rm:roots){if(std::find(excluded.begin(),excluded.end(),rm.move)!=excluded.end())continue;
        tick();U64 before=localNodes;int originalAlpha=alpha,score;Move m=rm.move;int t=token(p.board[m.from()],m.to());
        {
            Applied applied(p,m);stack[0].move=m;stack[0].token=t;
            if(!searched)score=-search(p,depth-1,-beta,-alpha,1,true);
            else{++stats.pvs;score=-search(p,depth-1,-alpha-1,-alpha,1,false);
                if(score>alpha&&score<beta){++stats.pvsResearch;score=-search(p,depth-1,-beta,-alpha,1,true);}}
            if(!p.nullBoundary&&p.claim_now()&&score<MateBound)score=std::max(score,0);
        }
        ++searched;rm.work+=localNodes-before;rm.depth=depth;
        rm.bound=score<=originalAlpha?Upper:score>=beta?Lower:Exact;
        if(rm.bound!=Exact)++rm.failures;
        rm.score=score;
        if(score>best){best=score;update_pv(0,m);rm.pv=stack[0].pv;}
        if(score>alpha)alpha=score;
        if(alpha>=beta)break;
    }
    return best;
}
Result Worker::run(Position p,const Limits& l,const Features& f,int workerId,int multiPV,const Reporter& report){
    limits=l;features=f;id=workerId;Result result;auto legal=p.legal_moves();std::vector<RootMove> roots;
    for(auto m:legal)if(!l.restrictMoves||std::find(l.searchmoves.begin(),l.searchmoves.end(),m)!=l.searchmoves.end()){RootMove rm;rm.move=m;roots.push_back(rm);}
    if(roots.empty()){result.score=legal.size?0:p.in_check()?-Mate:0;return result;}
    result.best=roots.front().move;result.pv.moves[0]=result.best;result.pv.size=1;result.score=evaluate(p);
    if(p.automatic_draw()){result.score=0;return result;}
    if(id)std::rotate(roots.begin(),roots.begin()+(id%roots.size()),roots.end());
    int stable=0,lastScore=result.score,swing=0;Move previousBest{};
    try{
        for(int depth=1;depth<=l.depth;++depth){
            auto beforeRoots=roots;
            std::vector<Move> selected;std::vector<Result> reports;
            int pvCount=std::min(multiPV,int(roots.size()));
            for(int pvIndex=0;pvIndex<pvCount;++pvIndex){
                int width=depth>=4&&pvIndex==0&&std::abs(lastScore)<MateBound?std::clamp(18+std::abs(swing)+15*(stable==0),18,300):Inf;
                int center=lastScore;int failures=0;
                for(;;){int alpha=std::max(-Inf,center-width),beta=std::min(Inf,center+width);
                    int score=root_search(p,roots,depth,alpha,beta,selected);
                    if(score<=alpha&&alpha>-Inf){++stats.aspirationLow;++failures;}
                    else if(score>=beta&&beta<Inf){++stats.aspirationHigh;++failures;}
                    else break;
                    width=failures>=4?2*Inf:width*2;
                    // Bring the fail-high candidate first before repeating PVS.
                    std::stable_sort(roots.begin(),roots.end(),[](const auto& a,const auto& b){return a.score>b.score;});
                }
                auto best=roots.end();for(auto it=roots.begin();it!=roots.end();++it)if(std::find(selected.begin(),selected.end(),it->move)==selected.end()&&(best==roots.end()||it->score>best->score))best=it;
                selected.push_back(best->move);
                Result r;r.best=best->move;r.score=best->score;r.depth=depth;r.pv=best->pv;reports.push_back(r);
            }
            for(auto& rm:roots){auto old=std::find_if(beforeRoots.begin(),beforeRoots.end(),[&](auto& o){return o.move==rm.move;});
                if(old!=beforeRoots.end()&&old->depth){rm.previous=old->score;rm.uncertainty=std::min(500,std::abs(rm.score-old->score));
                    bool samePV=rm.pv.size>1&&old->pv.size>1&&rm.pv.moves[1]==old->pv.moves[1];rm.pvChurn=samePV?std::max(0,old->pvChurn-1):old->pvChurn+1;
                    rm.stability=samePV&&rm.uncertainty<25?old->stability+1:0;}}
            // Bounded experimental budget: one decision probe and one refutation probe per iteration.
            // Never call these during MultiPV, helpers, or pure PVS baseline.
            if(!id&&pvCount==1&&depth>=4&&(features.decision||features.refutation)){
                auto candidate=roots.end();double priority=-1;int bestScore=reports[0].score;
                if(features.decision){for(auto it=roots.begin();it!=roots.end();++it)if(it->score>=bestScore-120&&std::abs(it->score)<MateBound){double value=it->priority(bestScore);if(value>priority){priority=value;candidate=it;}}}
                if(features.refutation&&!features.decision)candidate=std::find_if(roots.begin(),roots.end(),[&](auto& rm){return rm.move==reports[0].best;});
                if(candidate!=roots.end()){
                    auto probe=[&](bool refutation){U64 before=localNodes;Move m=candidate->move;int t=token(p.board[m.from()],m.to()),score;PV line;
                        {Applied applied(p,m);stack[0].move=m;stack[0].token=t;
                            // Auxiliary refutation search disables main selective shortcuts and considers every legal reply.
                            score=-search(p,depth,-Inf,Inf,1,true,false,{},0,false,refutation);
                            if(p.claim_now()&&score<MateBound)score=std::max(score,0);
                            if(stack[1].pv.size)refutations.store(p.key,stack[1].pv.moves[0]);
                            update_pv(0,m);line=stack[0].pv;}
                        if(refutation){++stats.refutationProbes;stats.refutationNodes+=localNodes-before;if(score<candidate->score-25){++stats.refutations;candidate->risk+=candidate->score-score;}}
                        else {++stats.decisionProbes;stats.decisionNodes+=localNodes-before;}
                        candidate->work+=localNodes-before;candidate->score=score;candidate->depth=depth+1;candidate->bound=Exact;candidate->pv=line;
                    };
                    if(features.decision)probe(false);
                    if(features.refutation)probe(true);
                    // Other root scores can be upper bounds. Resolve competition with a normal root pass.
                    U64 resolveStart=localNodes;root_search(p,roots,depth+1,-Inf,Inf,{});
                    if(features.decision)stats.decisionNodes+=localNodes-resolveStart;else stats.refutationNodes+=localNodes-resolveStart;
                    auto best=std::max_element(roots.begin(),roots.end(),[](auto& a,auto& b){return a.score<b.score;});reports[0].best=best->move;reports[0].score=best->score;reports[0].pv=best->pv;
                }
            }
            result=reports[0];if(p.can_claim(legal))result.score=std::max(0,result.score);stable=result.best==previousBest?stable+1:0;swing=result.score-lastScore;lastScore=result.score;previousBest=result.best;
            result.nodes=control.nodes.load();result.ms=control.time.elapsed();result.hashfull=tt.hashfull();result.seldepth=seldepth;result.stats=stats;result.roots=roots;
            if(report)for(int i=0;i<int(reports.size());++i){auto r=reports[i];r.nodes=result.nodes;r.ms=result.ms;r.hashfull=result.hashfull;r.seldepth=seldepth;report(r,i+1);}
            std::stable_sort(roots.begin(),roots.end(),[](const auto& a,const auto& b){return a.score>b.score;});
            bool competition=roots.size()>1&&roots[0].score-roots[1].score<25;
            if(!id&&control.time.soft_expired(stable,swing,competition))break;
            if(std::abs(result.score)>MateBound&&Mate-std::abs(result.score)<=depth)break;
        }
    }catch(const Interrupted&){}
    result.nodes=control.nodes.load();result.ms=control.time.elapsed();result.stats=stats;result.seldepth=seldepth;result.hashfull=tt.hashfull();
    return result;
}
}
