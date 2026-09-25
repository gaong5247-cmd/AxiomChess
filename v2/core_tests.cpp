#include "position.hpp"
#include "evaluation.hpp"
#include "axiom/chess.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
using namespace ax2;
void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
void verify(Position& p,axiom::Board& ref){
    std::vector<std::string>a,b;for(auto m:p.legal_moves())a.push_back(m.uci());for(auto m:ref.legal_moves_reference())b.push_back(m.uci());
    std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());if(a!=b)throw std::runtime_error("oracle mismatch "+p.fen());
    require(p.consistent(),"incremental state");require(p.fen()==ref.fen(),"FEN equivalence");
}
int main(){try{
    struct Test{const char* fen;int depth;U64 nodes;};
    Test tests[]={ {StartFen,4,197281},
      {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",3,97862},
      {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",4,43238},
      {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",3,9467},
      {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",3,62379}};
    for(auto t:tests){Position p(t.fen);auto n=perft(p,t.depth);std::cout<<"perft "<<t.depth<<" "<<n<<'\n';require(n==t.nodes,"perft");}
    const char* fixtures[]={StartFen,"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1","4k3/8/8/r4pPK/8/8/8/8 w - f6 0 1","1r5k/P7/8/8/8/8/8/7K w - - 0 1","4r1k1/8/8/8/1b6/8/8/4K3 w - - 0 1"};
    std::mt19937_64 rng(20260925);int positions=0;
    for(int game=0;game<100;++game){Position p(fixtures[game%5]);axiom::Board ref(p.fen());auto original=p;std::vector<std::pair<Move,Undo>> undo;
        for(int ply=0;ply<160;++ply){verify(p,ref);++positions;auto moves=p.legal_moves();if(!moves.size)break;auto m=moves[int(rng()%moves.size)];auto rm=ref.parse_move(m.uci());require(bool(rm),"oracle move");ref.push(*rm);Undo u;p.make(m,u);undo.emplace_back(m,u);}
        while(!undo.empty()){auto [m,u]=undo.back();undo.pop_back();p.unmake(m,u);require(p.consistent(),"unmake state");}
        require(p.fen()==original.fen()&&p.key==original.key&&p.context==original.context,"sequence restore");
    }
    Position ep("k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"),noep("k3r3/8/8/3pP3/8/8/8/4K3 w - - 0 1");require(ep.key==noep.key&&!ep.parse_move("e5d6"),"pinned EP");
    Position p;for(int i=0;i<4;++i)for(const char* s:{"g1f3","g8f6","f3g1","f6g8"}){Undo u;p.make(p.parse_move(s),u);}require(p.repetitions()==5&&p.automatic_draw(),"fivefold");
    Undo u;p.make_null(u);require(p.repetitions()==0&&p.consistent(),"null repetition barrier");p.unmake_null(u);require(p.repetitions()==5&&p.consistent(),"null restore");
    Position repeat;for(const char* s:{"g1f3","g8f6","f3g1","f6g8","g1f3","g8f6","f3g1"}){Undo state;repeat.make(repeat.parse_move(s),state);}
    require(!repeat.claim_now()&&repeat.can_claim(repeat.legal_moves()),"intended third repetition");require(repeat.consistent(),"claim probe restores state");
    Position exchange("r6k/p7/8/8/8/8/8/R6K w - - 0 1");auto capture=exchange.parse_move("a1a7");require(bool(capture),"SEE fixture legal");require(see_ge(exchange,capture,-400)&&!see_ge(exchange,capture,-399),"SEE losing capture threshold");
    Position kingCapture("1k6/q7/8/8/8/8/5B2/R6K w - - 0 1");capture=kingCapture.parse_move("a1a7");require(bool(capture)&&see_ge(kingCapture,capture,950)&&!see_ge(kingCapture,capture,951),"SEE illegal king recapture excluded");
    Position legalEp("k7/8/8/3pP3/8/8/8/4K3 w - d6 0 1");require(see_ge(legalEp,legalEp.parse_move("e5d6"),100),"SEE en passant");
    require(from_tt(to_tt(Mate-8,5),2)==Mate-5,"TT mate normalization");
    std::cout<<"PASS oracle fuzz positions="<<positions<<" seed=20260925\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
