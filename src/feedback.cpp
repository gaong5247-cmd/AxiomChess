#include "axiom/engine.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace axiom {
namespace {
using Flag=std::pair<const char*,bool Features::*>;
constexpr Flag flags[]={
    {"correction",&Features::correction},{"continuation",&Features::continuation},
    {"capture_history",&Features::capture_history},{"countermove",&Features::countermove},
    {"singular",&Features::singular},{"verified_null",&Features::verified_null},
    {"probcut",&Features::probcut},{"dynamic_lmr",&Features::dynamic_lmr},
    {"history_pruning",&Features::history_pruning},{"see_pruning",&Features::see_pruning},
    {"mate_distance",&Features::mate_distance},{"iid",&Features::iid},
    {"tt_policy",&Features::tt_policy},{"time_management",&Features::time_management},
    {"strategic_eval",&Features::strategic_eval},{"king_safety",&Features::king_safety},
    {"search_safety",&Features::search_safety},{"pawn_cache",&Features::pawn_cache},{"rfp",&Features::rfp},
    {"trend_safety",&Features::trend_safety},{"calibrated_lmr",&Features::calibrated_lmr},{"adaptive_time",&Features::adaptive_time}};
std::uint64_t scramble(std::uint64_t x) { x^=x>>30; x*=0xbf58476d1ce4e5b9ULL; x^=x>>27; x*=0x94d049bb133111ebULL; return x^(x>>31); }
std::size_t pair_key(int previous,int current) { return scramble(std::uint64_t(previous)*1024+current)&65535; }
std::size_t capture_key(int piece,int to,int victim) { return (std::size_t(Feedback::token(piece,to)-1)*7)+std::abs(victim); }
}
bool Features::set(const std::string& name,bool enabled) { if(name=="all") { all(enabled); return true; } for(auto [n,f]:flags) if(name==n) { this->*f=enabled; return true; } return false; }
void Features::all(bool enabled) { for(auto [n,f]:flags) { (void)n; this->*f=enabled; } }
void Features::middlegame() { all(false); correction=continuation=capture_history=countermove=dynamic_lmr=king_safety=strategic_eval=search_safety=pawn_cache=true; }
std::string Features::names() const { std::string out; for(auto [n,f]:flags) if(this->*f) { if(!out.empty()) out+=','; out+=n; } return out; }
TranspositionTable::TranspositionTable(std::size_t mb) {
    // Reserve approximate storage for exact repetition context strings too.
    buckets_.resize(std::max<std::size_t>(1,std::min<std::size_t>(mb,1024)*1024*1024/(sizeof(Bucket)+4*256)));
}
std::optional<TTEntry> TranspositionTable::probe(std::uint64_t key,const std::string& context) const {
    auto index=key%buckets_.size(); std::lock_guard<std::mutex> guard(locks_[index%locks_.size()]);
    for(const auto& entry:buckets_[index].entries) if(entry.depth>=0 && entry.key==key && entry.context==context) return entry; return {};
}
void TranspositionTable::store(TTEntry entry,bool policy) {
    auto index=entry.key%buckets_.size(); std::lock_guard<std::mutex> guard(locks_[index%locks_.size()]);
    auto& entries=buckets_[index].entries; entry.generation=generation_;
    auto quality=[&](const TTEntry& e) { return e.depth+4*e.pv+6*(e.bound==Bound::Exact)-8*static_cast<int>(std::min(16u,generation_-e.generation)); };
    TTEntry* target=&entries[0];
    for(auto& old:entries) {
        if(old.depth>=0 && old.key==entry.key && old.context==entry.context) {
            if(policy && quality(old)>quality(entry)+3) { old.generation=generation_; old.pv=old.pv || entry.pv; return; }
            target=&old; break;
        }
        if(old.depth<0) { target=&old; break; }
        if(policy && quality(old)<quality(*target)) target=&old;
    }
    *target=std::move(entry);
}
void TranspositionTable::clear() { for(std::size_t i=0;i<buckets_.size();++i) { std::lock_guard<std::mutex> guard(locks_[i%locks_.size()]); for(auto& e:buckets_[i].entries) e=TTEntry{}; } }
Feedback::Feedback():captures_(768*7) { for(auto& t:corrections_) t.resize(8192); for(auto& t:continuations_) t.resize(65536); }
void Feedback::clear() { for(auto& t:corrections_) std::fill(t.begin(),t.end(),std::int16_t{0}); for(auto& t:continuations_) std::fill(t.begin(),t.end(),std::int16_t{0}); std::fill(captures_.begin(),captures_.end(),std::int16_t{0}); counters_.fill(Move{}); }
void Feedback::update(std::int16_t& value,int bonus,int limit) { bonus=std::clamp(bonus,-limit,limit); int next=value+bonus-int(value)*std::abs(bonus)/limit; value=static_cast<std::int16_t>(std::clamp(next,-limit,limit)); }
int Feedback::token(int piece,int to) { if(!piece || !valid(to)) return 0; int index=std::abs(piece)-1+(piece<0?6:0); return 1+index*64+(to>>4)*8+(to&7); }
std::array<std::size_t,5> Feedback::correction_keys(const Board& b,int previous) const {
    std::uint64_t pawn=0,minor=0,nonpawn=0,material=0; int counts[13]{};
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]) {
        int p=b.squares[s]; auto h=scramble(std::uint64_t(p+6)*128+s+17); ++counts[p+6];
        if(std::abs(p)==Pawn) pawn^=h;
        if(std::abs(p)==Knight || std::abs(p)==Bishop) minor^=h;
        if(std::abs(p)!=Pawn && std::abs(p)!=King) nonpawn^=h;
    }
    for(int i=0;i<13;++i) material^=scramble(i*32+counts[i]+5000);
    std::uint64_t stm=b.side==White?0:4096;
    return {std::size_t((pawn&4095)+stm),std::size_t((material&4095)+stm),std::size_t((minor&4095)+stm),std::size_t((nonpawn&4095)+stm),std::size_t((scramble(previous)&4095)+stm)};
}
std::array<int,5> Feedback::correction_components(const Board& b,int previous) const {
    auto keys=correction_keys(b,previous); std::array<int,5> values{};
    for(int i=0;i<5;++i) values[i]=corrections_[i][keys[i]];
    return values;
}
int Feedback::correction(const Board& b,int previous,unsigned mask) const {
    auto values=correction_components(b,previous); int total=0,count=0;
    for(int i=0;i<5;++i) if(mask&(1u<<i)) { total+=values[i]; ++count; }
    return count?std::clamp(total/(16*count),-256,256):0;
}
void Feedback::update_correction(const Board& b,int previous,int error,int depth) {
    auto keys=correction_keys(b,previous);
    int target=std::clamp(error,-256,256)*16,weight=std::clamp(depth,1,12);
    // Bounded exponential averaging: repeated +80cp errors converge to +80cp,
    // not to the table's saturation limit as a constant history bonus would.
    for(int i=0;i<5;++i) {
        auto& value=corrections_[i][keys[i]];
        value=static_cast<std::int16_t>(std::clamp(int(value)+(target-int(value))*weight/64,-4096,4096));
    }
}
int Feedback::continuation(int previous,int current,int distance) const { return previous && current?continuations_[distance][pair_key(previous,current)]:0; }
void Feedback::update_continuation(int previous,int current,int distance,int bonus) { if(previous && current) update(continuations_[distance][pair_key(previous,current)],bonus); }
int Feedback::capture_score(int piece,int to,int victim) const { return captures_[capture_key(piece,to,victim)]; }
void Feedback::update_capture(int piece,int to,int victim,int bonus) { update(captures_[capture_key(piece,to,victim)],bonus); }
Move Feedback::counter(int previous) const { return counters_[previous]; }
void Feedback::set_counter(int previous,Move move) { if(previous) counters_[previous]=move; }
}
