#include "search.hpp"
namespace ax2 {
int Worker::qsearch(Position& p,int alpha,int beta,int ply,int qply){
    tick();++stats.qnodes;seldepth=std::max(seldepth,ply);stack[ply].pv.size=0;
    bool check=p.in_check();auto moves=p.legal_moves();
    if(!moves.size)return check?-Mate+ply:0;
    if(p.insufficient()||(!p.nullBoundary&&p.automatic_draw()))return 0;
    bool claim=p.can_claim(moves);
    int stand=evaluate(p);
    if(ply>=MaxPly-2){if(check)throw Interrupted{};return claim?std::max(0,stand):stand;}
    int best=check?-Inf:stand;if(claim){best=std::max(best,0);alpha=std::max(alpha,0);if(alpha>=beta)return alpha;}
    if(!check){if(stand>=beta)return stand;alpha=std::max(alpha,stand);if(qply>=24)return best;}
    order(p,moves,{},ply);int prev=ply?stack[ply-1].token:-1;(void)prev;
    for(auto m:moves){bool cap=p.capture(m);int victim=p.victim(m);if(!check&&!cap&&!m.promo()&&qply>=1)continue;
        bool good=!features.see||!cap||see_ge(p,m,0);int t=token(p.board[m.from()],m.to());int score;
        {
            Applied applied(p,m);bool givesCheck=p.in_check();
            if(!check&&!cap&&!m.promo()&&!givesCheck)continue;
            if(!check&&!givesCheck&&!m.promo()){
                if(features.delta&&stand+values[victim]+180<alpha){++stats.delta;continue;}
                if(!good){++stats.see;continue;}
            }
            stack[ply].move=m;stack[ply].token=t;
            score=-qsearch(p,-beta,-alpha,ply+1,qply+1);
            // A draw may be claimed by the mover with the intended move, except mate.
            if(!p.nullBoundary&&p.claim_now()&&score<MateBound)score=std::max(score,0);
        }
        if(score>best)best=score;
        if(score>alpha){alpha=score;update_pv(ply,m);if(alpha>=beta)break;}
    }
    return best;
}
}
