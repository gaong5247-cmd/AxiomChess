#pragma once
#include "types.hpp"
#include <atomic>
#include <chrono>
#include <vector>
namespace ax2 {
using Clock=std::chrono::steady_clock;
inline std::int64_t now_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();}
struct Limits {
    int depth=MaxPly-8,movetime=0,movestogo=0,overhead=50;
    std::int64_t time[2]={-1,-1},inc[2]={0,0};
    U64 nodes=0;
    bool ponder=false,infinite=false,restrictMoves=false;
    std::vector<Move> searchmoves;
};
class TimeManager {
    std::atomic<std::int64_t> start{0},deadline{0};
    std::atomic_bool pondering{false};
    std::int64_t soft=0,hard=0;
public:
    void reset(const Limits& l,int side);
    void ponderhit();
    bool is_pondering() const{return pondering.load(std::memory_order_relaxed);}
    bool expired() const{auto d=deadline.load(std::memory_order_relaxed);return d&&now_ms()>=d;}
    bool soft_expired(int stability,int swing,bool competition) const;
    std::int64_t elapsed() const{return std::max<std::int64_t>(0,now_ms()-start.load());}
};
struct Control {
    std::atomic_bool stop{false};
    std::atomic<U64> nodes{0};
    TimeManager time;
};
}
