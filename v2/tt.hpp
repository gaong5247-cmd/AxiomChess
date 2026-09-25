#pragma once
#include "types.hpp"
#include <memory>
#include <mutex>
#include <optional>
namespace ax2 {
enum Bound { NoBound, Upper, Lower, Exact };
struct TTEntry {
    U64 key=0;
    Move move{};
    std::int16_t score=0,eval=0;
    std::uint8_t depth=0,flags=0;
    Bound bound() const{return Bound(flags&3);}
    bool pv() const{return flags&4;}
    int generation() const{return flags>>3;}
};
static_assert(sizeof(TTEntry)==16);
class TT {
    struct alignas(64) Cluster {std::array<TTEntry,4> entries{};};
    static_assert(sizeof(Cluster)==64);
    std::unique_ptr<Cluster[]> table;
    std::size_t size=0;
    mutable std::array<std::mutex,4096> locks;
    int generation=0;
public:
    explicit TT(int mb=32){resize(mb);}
    void resize(int mb);
    void clear();
    void new_search(){generation=(generation+1)&31;}
    std::optional<TTEntry> probe(U64 key) const;
    void store(U64 key,Move move,int score,int eval,int depth,Bound bound,bool pv);
    int hashfull() const;
};
}
