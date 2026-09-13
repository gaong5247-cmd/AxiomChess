#include "axiom/engine.hpp"
#include <algorithm>
namespace axiom {
TranspositionTable::TranspositionTable(std::size_t mb) {
    // Context strings remain exact. This is responsibility separation, not
    // a compact-token substitution that could weaken repetition identity.
    buckets_.resize(std::max<std::size_t>(1,std::min<std::size_t>(mb,1024)*1024*1024/(sizeof(Bucket)+4*256)));
}
std::optional<TTEntry> TranspositionTable::probe(std::uint64_t key,const std::string& context) const {
    auto index=key%buckets_.size();std::lock_guard<std::mutex> guard(locks_[index%locks_.size()]);
    for(const auto& entry:buckets_[index].entries)if(entry.depth>=0&&entry.key==key&&entry.context==context)return entry;return {};
}
void TranspositionTable::store(TTEntry entry,bool policy) {
    auto index=entry.key%buckets_.size();std::lock_guard<std::mutex> guard(locks_[index%locks_.size()]);
    auto& entries=buckets_[index].entries;entry.generation=generation_;
    auto quality=[&](const TTEntry& e){return e.depth+4*e.pv+6*(e.bound==Bound::Exact)-8*static_cast<int>(std::min(16u,generation_-e.generation));};
    TTEntry* target=&entries[0];
    for(auto& old:entries){
        if(old.depth>=0&&old.key==entry.key&&old.context==entry.context){
            if(policy&&quality(old)>quality(entry)+3){old.generation=generation_;old.pv=old.pv||entry.pv;return;}
            target=&old;break;
        }
        if(old.depth<0){target=&old;break;}
        if(policy&&quality(old)<quality(*target))target=&old;
    }
    *target=std::move(entry);
}
void TranspositionTable::clear(){for(std::size_t i=0;i<buckets_.size();++i){std::lock_guard<std::mutex> guard(locks_[i%locks_.size()]);for(auto& e:buckets_[i].entries)e=TTEntry{};}}
int TranspositionTable::hashfull() const {
    const auto count=std::min<std::size_t>(250,buckets_.size());int used=0;
    for(std::size_t i=0;i<count;++i){std::lock_guard<std::mutex> guard(locks_[i%locks_.size()]);for(const auto& e:buckets_[i].entries)used+=e.depth>=0;}
    return count?static_cast<int>(used*1000/(count*4)):0;
}
}
