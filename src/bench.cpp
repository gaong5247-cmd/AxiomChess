#include "axiom/release.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace axiom {
std::string run_bench(int depth,std::uint64_t budget) {
    if(depth<1||depth>20)throw std::invalid_argument("bench depth must be 1..20");
    constexpr const char* positions[]={Board::StartFen,
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      "rn1qkb1r/pbpp1ppp/1p2pn2/8/2PP4/5NP1/PP2PP1P/RNBQKB1R w KQkq - 0 1"};
    std::uint64_t total=0,checksum=14695981039346656037ULL;
    auto mix=[&](const std::string& s){for(unsigned char c:s){checksum^=c;checksum*=1099511628211ULL;}checksum^=255;checksum*=1099511628211ULL;};
    auto start=std::chrono::steady_clock::now();
    for(auto fen:positions){auto engine=std::make_unique<Search>(32);Limits l;l.depth=depth;l.nodes=budget;l.milliseconds=0;l.proof_nodes=0;l.features=production_features();std::atomic_bool stop=false;
        auto r=engine->run(Board(fen),l,stop);total+=r.nodes;mix(fen);mix(r.best.uci());mix(std::to_string(r.score));mix(std::to_string(r.depth));mix(std::to_string(r.nodes));
        for(const auto& m:r.moves)if(m.move==r.best)for(auto pv:m.pv)mix(pv.uci());
    }
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    std::ostringstream out;out<<"{\"positions\":5,\"depth_limit\":"<<depth<<",\"node_limit_per_position\":"<<budget<<",\"nodes\":"<<total<<",\"time_ms\":"<<ms<<",\"nps\":"<<total*1000/std::max<long long>(1,ms)<<",\"checksum\":\""<<std::hex<<checksum<<"\",\"policy\":\"v1-baseline-middlegame-proof0-threads1-hash32\"}";return out.str();
}
}
