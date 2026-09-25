#include "search.hpp"
#include "tablebase.hpp"
#include <cmath>
namespace ax2 {
namespace {
const auto reductions=[] {std::array<std::array<int,256>,MaxPly> table{};for(int d=1;d<MaxPly;++d)for(int n=1;n<256;++n)table[d][n]=int(std::log(double(d))*std::log(double(n))/2.2);return table;}();
}
void Worker::tick(){
    if(control.stop.load(std::memory_order_relaxed))throw Interrupted{};
    if((localNodes&255)==0&&control.time.expired()){control.stop=true;throw Interrupted{};}
    if(limits.nodes){U64 current=control.nodes.load(std::memory_order_relaxed);for(;;){if(current>=limits.nodes)throw Interrupted{};if(control.nodes.compare_exchange_weak(current,current+1,std::memory_order_relaxed))break;}}
    else control.nodes.fetch_add(1,std::memory_order_relaxed);
    ++localNodes;
}
int Worker::search(Position& p,int depth,int alpha,int beta,int ply,bool pv,bool nullAllowed,Move excluded,int extensions,bool cut,bool auxiliary){
    if(depth<=0)return qsearch(p,alpha,beta,ply,0);
    tick();seldepth=std::max(seldepth,ply);stack[ply].pv.size=0;
    bool check=p.in_check();auto moves=p.legal_moves();if(!moves.size)return check?-Mate+ply:0;
    if(p.insufficient()||(!p.nullBoundary&&p.automatic_draw()))return 0;
    bool claim=p.can_claim(moves);
    if(ply>=MaxPly-2){if(check)throw Interrupted{};int eval=evaluate(p);return claim?std::max(0,eval):eval;}
    alpha=std::max(alpha,-Mate+ply);beta=std::min(beta,Mate-ply-1);if(alpha>=beta)return alpha;
    int originalAlpha=alpha;if(claim){alpha=std::max(alpha,0);if(alpha>=beta)return 0;}
    bool trusted=!excluded&&!auxiliary&&!p.nullBoundary;
    U64 key=p.tt_key();auto entry=trusted?tt.probe(key):std::optional<TTEntry>{};Move ttMove=entry?entry->move:Move{};
    if(entry){++stats.ttHits;int value=from_tt(entry->score,ply);if(!pv&&entry->depth>=depth&&(entry->bound()==Exact||(entry->bound()==Lower&&value>=beta)||(entry->bound()==Upper&&value<=alpha))){++stats.ttCuts;return value;}}
    if(trusted&&tb&&depth>=2)if(auto score=tb->wdl(p)){++stats.tbHits;return *score;}
    int eval=entry?entry->eval:evaluate(p);stack[ply].eval=check?Inf:eval;
    bool improving=ply>=2&&stack[ply-2].eval!=Inf&&eval>stack[ply-2].eval;
    bool unstable=ply>=2&&stack[ply-2].eval!=Inf&&std::abs(eval-stack[ply-2].eval)>100;
    bool endgame=p.phase<=6;
    bool pruning=trusted&&!pv&&!check&&!claim&&std::abs(beta)<MateBound&&moves.size>1;
    if(pruning&&!endgame&&!unstable){
        if(features.rfp&&depth<=3&&eval-100*depth>=beta){++stats.rfp;return eval-100*depth;}
        if(features.razor&&depth<=2&&eval+220*depth<alpha){int q=qsearch(p,alpha,beta,ply,0);if(q<=alpha){++stats.razor;return q;}}
    }
    if(pruning&&features.nullMove&&nullAllowed&&depth>=3&&eval>=beta&&p.nonpawn(p.side)&&p.halfmove<80){
        bool advancedPawns=(p.bb[White][Pawn]&0x00FF000000000000ULL)||(p.bb[Black][Pawn]&0x000000000000FF00ULL);
        // Pawn-only positions and promotion races never use a null shortcut.
        if(!advancedPawns){++stats.nullTries;int reduction=2+depth/4+std::min(2,(eval-beta)/200),score;
            stack[ply].move={};stack[ply].token=-1;
            U64 nullStart=localNodes;
            {Applied applied(p,{});score=-search(p,std::max(0,depth-reduction-1),-beta,-beta+1,ply+1,false,false,{},extensions,!cut,true);}
            stats.nullNodes+=localNodes-nullStart;
            if(score>=beta){bool accept=true;if(depth>=6||endgame){++stats.nullVerify;int verified=search(p,std::max(1,depth-reduction),beta-1,beta,ply,false,false,{},extensions,cut,true);accept=verified>=beta;score=std::min(score,verified);if(!accept)++stats.nullReject;}
                if(accept){++stats.nullCuts;return std::min(score,MateBound-1);}}
        }
    }
    int singular=0;
    if(trusted&&features.singular&&entry&&ttMove&&depth>=6&&entry->depth>=depth-2&&entry->bound()!=Upper&&std::abs(entry->score)<MateBound&&extensions<6){
        ++stats.singularTries;int threshold=from_tt(entry->score,ply)-2*depth;
        U64 singularStart=localNodes;int alternative=search(p,(depth-1)/2,threshold-1,threshold,ply,false,false,ttMove,extensions,cut,true);stats.singularNodes+=localNodes-singularStart;
        if(alternative<threshold){singular=1;++stats.singularExtensions;}
    }
    stack[ply].eval=check?Inf:eval;stack[ply].pv.size=0;
    Move refute=features.refutation?refutations.probe(p.key):Move{};order(p,moves,ttMove,ply,refute);
    if(pruning&&features.probcut&&!endgame&&depth>=5&&!unstable){int raised=beta+180;int tried=0;
        for(auto m:moves)if(p.capture(m)&&!m.promo()&&see_ge(p,m,raised-eval)&&tried++<3){++stats.probcutTries;int score,t=token(p.board[m.from()],m.to());
            U64 probcutStart=localNodes;
            {Applied applied(p,m);stack[ply].token=t;stack[ply].move=m;score=-qsearch(p,-raised,-raised+1,ply+1,0);if(score>=raised)score=-search(p,depth-4,-raised,-raised+1,ply+1,false,false,{},extensions,true,true);}
            stats.probcutNodes+=localNodes-probcutStart;
            if(score>=raised&&score<MateBound){++stats.probcut;return score-180;}
        }
    }
    int best=claim?0:-Inf,searched=0,index=0,prev=ply?stack[ply-1].token:-1;Move bestMove{};MoveList tried;
    for(auto m:moves){if(m==excluded)continue;++index;bool cap=p.capture(m),quiet=!cap&&!m.promo();int hist=histories->quiet(p,m,prev);
        bool killer=m==histories->killers[ply][0]||m==histories->killers[ply][1];bool protectedMove=m==ttMove||killer||m==refute;
        bool goodSee=!features.see||see_ge(p,m,quiet?-70*depth:-30*depth*depth);
        int t=token(p.board[m.from()],m.to()),score,extension=(m==ttMove?singular:0),reduction=0;
        {
            Applied applied(p,m);bool givesCheck=p.in_check();
            if(pruning&&searched>0&&!endgame&&!unstable&&!givesCheck&&!m.promo()&&!protectedMove){
                if(features.futility&&quiet&&depth<=3&&eval+110*depth<=alpha){++stats.futility;continue;}
                if(features.moveCount&&quiet&&depth<=3&&index>4+depth*depth){++stats.moveCount;continue;}
                if(features.history&&quiet&&depth<=3&&hist<-4000*depth){++stats.history;continue;}
                if(features.see&&depth<=5&&!goodSee){++stats.see;continue;}
            }
            if(check&&extensions<8&&extension==0)extension=1;
            if(extensions>=8)extension=0;
            int nextDepth=depth-1+extension;
            stack[ply].move=m;stack[ply].token=t;
            if(features.lmr&&!auxiliary&&depth>=3&&searched>=3&&quiet&&!check&&!givesCheck&&!protectedMove&&!extension){
                reduction=reductions[std::min(depth,MaxPly-1)][std::min(index,255)]+(cut?1:0)-(pv?1:0)-(improving?1:0)-(unstable?1:0)-hist/5000;
                reduction=std::clamp(reduction,0,std::max(0,nextDepth-1));if(reduction)++stats.lmr;
            }
            if(searched==0)score=-search(p,nextDepth,-beta,-alpha,ply+1,pv,true,{},extensions+extension,false,auxiliary);
            else {
                ++stats.pvs;score=-search(p,nextDepth-reduction,-alpha-1,-alpha,ply+1,false,true,{},extensions+extension,true,auxiliary);
                if(reduction&&score>alpha){++stats.lmrResearch;U64 reStart=localNodes;score=-search(p,nextDepth,-alpha-1,-alpha,ply+1,false,true,{},extensions+extension,!cut,auxiliary);stats.lmrResearchNodes+=localNodes-reStart;}
                // At unstable PV decisions, a near-alpha upper bound can hide a
                // changed choice. Spend one bounded extra ply on this competitor.
                // This is an experimental bound heuristic, never an exact proof.
                if(features.decision&&pv&&!auxiliary&&unstable&&depth>=5&&extensions+extension<6&&score<=alpha&&score>alpha-25){
                    ++stats.decisionInternalProbes;U64 before=localNodes;++nextDepth;++extension;
                    score=-search(p,nextDepth,-alpha-1,-alpha,ply+1,false,false,{},extensions+extension,true,false);
                    stats.decisionInternalNodes+=localNodes-before;
                }
                if(pv&&score>alpha&&score<beta){++stats.pvsResearch;score=-search(p,nextDepth,-beta,-alpha,ply+1,true,true,{},extensions+extension,false,auxiliary);}
            }
            if(!p.nullBoundary&&p.claim_now()&&score<MateBound)score=std::max(score,0);
        }
        ++searched;tried.add(m);
        if(score>best){best=score;bestMove=m;}
        if(score>alpha){alpha=score;update_pv(ply,m);if(alpha>=beta){
            if(trusted){int bonus=std::min(1600,depth*depth*32);histories->reward(p,m,prev,bonus);
                for(auto old:tried)if(old!=m&&p.capture(old)==cap)histories->reward(p,old,prev,-bonus/2);
                if(quiet){histories->killers[ply][1]=histories->killers[ply][0];histories->killers[ply][0]=m;if(prev>=0)histories->counters[prev]=m;}
                if(features.refutation)refutations.store(p.key,m);
            }break;
        }}
    }
    if(!searched)return excluded?alpha:best;
    if(trusted)tt.store(key,bestMove,to_tt(best,ply),eval,depth,best>=beta?Lower:best>originalAlpha?Exact:Upper,pv);
    return best;
}
}
