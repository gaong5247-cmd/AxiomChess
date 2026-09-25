#include "search.hpp"
namespace ax2 {
void Worker::order(const Position& p,MoveList& moves,Move ttMove,int ply,Move refute){
    struct Scored {Move move;int score;};std::array<Scored,256> list{};int prev=ply?stack[ply-1].token:-1;
    for(int i=0;i<moves.size;++i){Move m=moves[i];int score;
        if(m==ttMove)score=2000000;
        else if(p.capture(m))score=(see_ge(p,m,0)?1000000:-100000)+16*values[p.victim(m)]-values[type(p.board[m.from()])]+histories->captures[p.side][type(p.board[m.from()])][m.to()][p.victim(m)];
        else if(m.promo())score=900000+values[m.promo()];
        else if(m==histories->killers[ply][0])score=800000;
        else if(m==histories->killers[ply][1])score=790000;
        else if(prev>=0&&m==histories->counters[prev])score=780000;
        else if(m==refute)score=770000;
        else score=histories->quiet(p,m,prev);
        if(m.promo())score+=50000;
        if(id&&m!=ttMove)score+=(m.v*13+id*17)%31;
        list[i]={m,score};
    }
    // Allocation-free stable insertion sort; deterministic tie ordering.
    for(int i=1;i<moves.size;++i){auto value=list[i];int j=i;while(j&&list[j-1].score<value.score){list[j]=list[j-1];--j;}list[j]=value;}
    for(int i=0;i<moves.size;++i)moves.data[i]=list[i].move;
}
void Worker::update_pv(int ply,Move m){auto& pv=stack[ply].pv;const auto& child=stack[ply+1].pv;pv.moves[0]=m;pv.size=std::min(MaxPly-1,child.size+1);std::copy_n(child.moves.begin(),pv.size-1,pv.moves.begin()+1);}
}
