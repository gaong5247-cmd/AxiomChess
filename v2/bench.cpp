#include "bench.hpp"
namespace ax2 {
void bench(std::ostream& out,int depth,U64 budget,const Features& f){
    const char* fens[]={StartFen,"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1","8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1","r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1","rn1qkb1r/pbpp1ppp/1p2pn2/8/2PP4/5NP1/PP2PP1P/RNBQKB1R w KQkq - 0 1"};
    U64 nodes=0,checksum=14695981039346656037ULL;auto start=now_ms();int i=0;
    auto mixText=[&](std::string s){for(unsigned char c:s){checksum^=c;checksum*=1099511628211ULL;}};
    for(auto fen:fens){auto e=std::make_unique<Engine>(32);Position p(fen);Limits l;l.depth=depth;l.nodes=budget;e->prepare(l,p.side);auto r=e->run(p,l,f);
        nodes+=r.nodes;mixText(r.best.uci()+":"+std::to_string(r.score)+":"+std::to_string(r.nodes)+":"+std::to_string(r.depth));
        out<<"{\"position\":"<<i++<<",\"nodes\":"<<r.nodes<<",\"time_ms\":"<<r.ms<<",\"nps\":"<<r.nodes*1000/std::max<std::int64_t>(1,r.ms)<<",\"hashfull\":"<<r.hashfull<<",\"depth\":"<<r.depth<<",\"bestmove\":\""<<r.best.uci()<<"\",\"score\":"<<r.score<<",\"stats\":\""<<r.stats.text()<<"\"}\n";
    }
    auto ms=now_ms()-start;out<<"{\"positions\":5,\"depth_limit\":"<<depth<<",\"node_limit\":"<<budget<<",\"nodes\":"<<nodes<<",\"time_ms\":"<<ms<<",\"nps\":"<<nodes*1000/std::max<std::int64_t>(1,ms)<<",\"checksum\":\""<<std::hex<<checksum<<std::dec<<"\"}\n";
}
}
