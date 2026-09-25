#include "search.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ax2;
void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
void legal_pv(Position p,const PV& pv){for(int i=0;i<pv.size;++i){auto m=p.parse_move(pv.moves[i].uci());require(bool(m),"PV legality");Undo u;p.make(m,u);}}
int main(){try{
    auto engine=std::make_unique<Engine>(4);Features f;Limits l;l.depth=5;l.nodes=50000;Position p;
    engine->prepare(l,p.side);auto r=engine->run(p,l,f);require(r.depth>=3&&r.nodes<=l.nodes,"bounded search");require(bool(p.parse_move(r.best.uci())),"legal bestmove");legal_pv(p,r.pv);
    engine->clear();engine->prepare(l,p.side);auto again=engine->run(p,l,f);require(r.nodes==again.nodes&&r.score==again.score&&r.best==again.best,"deterministic cold search");
    l.restrictMoves=true;l.searchmoves={p.parse_move("e2e4")};engine->prepare(l,p.side);r=engine->run(p,l,f);require(r.best==l.searchmoves[0],"searchmoves");
    l.restrictMoves=false;l.nodes=10000;l.depth=100;engine->prepare(l,p.side);r=engine->run(p,l,f,4);require(r.nodes<=l.nodes,"shared SMP node budget");legal_pv(p,r.pv);
    l.nodes=0;l.depth=100;engine->prepare(l,p.side);std::thread t([&]{r=engine->run(p,l,f,2);});std::this_thread::sleep_for(std::chrono::milliseconds(30));auto start=now_ms();engine->stop();t.join();require(now_ms()-start<1000,"stop responsiveness");
    for(const char* fen:{"7k/6Q1/6K1/8/8/8/8/8 b - - 150 1","7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"}){Position terminal(fen);engine->prepare(l,terminal.side);r=engine->run(terminal,l,f);require(!r.best,"terminal no move");require(r.score==(terminal.in_check()?-Mate:0),"terminal score/mate before rule75");}
    Position mate("7k/8/5KQ1/8/8/8/8/8 w - - 99 1");l.depth=4;engine->prepare(l,mate.side);r=engine->run(mate,l,f);require(r.score>MateBound,"mate before draw claim");legal_pv(mate,r.pv);
    Position claim("7k/8/8/8/8/8/q7/7K w - - 99 1");require(claim.can_claim(claim.legal_moves()),"intended rule50 claim");engine->prepare(l,claim.side);r=engine->run(claim,l,f);require(r.score>=0,"claim is an option in losing position");
    l.depth=4;l.nodes=50000;f.decision=f.refutation=true;engine->prepare(l,p.side);r=engine->run(p,l,f);legal_pv(p,r.pv);
    require(r.stats.decisionProbes>0&&r.stats.refutationProbes>0,"experimental probes execute");
    f.razor=f.moveCount=f.history=f.probcut=true;l.depth=8;l.nodes=30000;
    for(const char* fen:{StartFen,"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1","8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"}){Position pos(fen);engine->clear();engine->prepare(l,pos.side);r=engine->run(pos,l,f);require(r.nodes<=l.nodes&&bool(pos.parse_move(r.best.uci())),"all features legal bounded result");legal_pv(pos,r.pv);}
    TT tt(1);tt.store(42,Move(1,18),to_tt(Mate-8,5),23,10,Exact,true);auto entry=tt.probe(42);require(entry&&from_tt(entry->score,2)==Mate-5&&entry->eval==23,"TT fields");
    std::atomic_bool coherent=true;std::vector<std::thread> racers;
    for(int id=0;id<4;++id)racers.emplace_back([&,id]{for(int i=0;i<10000;++i){U64 key=mix(U64(i+id*10000));int value=int(key%10000);tt.store(key,Move(1,18),value,-value,i%100,Exact,true);if(auto e=tt.probe(key))if(e->score!=value||e->eval!=-value)coherent=false;}});
    for(auto& racer:racers)racer.join();require(coherent,"concurrent TT coherent records");
    std::cout<<"PASS search, PV, mate, determinism, TT, node limits, SMP and stop\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
