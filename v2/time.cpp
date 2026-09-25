#include "time.hpp"
namespace ax2 {
void TimeManager::reset(const Limits& l,int side){
    auto t=now_ms();start=t;soft=hard=0;pondering=l.ponder;
    if(l.movetime>0)soft=hard=std::max(1,l.movetime-l.overhead);
    else if(l.time[side]>=0&&!l.infinite){auto available=std::max<std::int64_t>(1,l.time[side]-l.overhead);auto moves=l.movestogo?l.movestogo:30;
        soft=std::clamp(l.time[side]/moves+l.inc[side]*3/4,std::int64_t(1),available);hard=std::min(available,soft*4);}
    deadline=!l.ponder&&hard?t+hard:0;
}
void TimeManager::ponderhit(){auto t=now_ms();start=t;if(hard)deadline=t+hard;pondering=false;}
bool TimeManager::soft_expired(int stability,int swing,bool competition) const {
    if(!soft||is_pondering())return false;
    double factor=1.0+std::min(1.0,std::abs(swing)/100.0)+(competition?0.25:0.0)-std::min(0.4,stability*0.08);
    return elapsed()>=std::int64_t(soft*factor);
}
}
