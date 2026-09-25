#include "tt.hpp"
namespace ax2 {
void TT::resize(int mb){auto n=std::bit_floor(std::size_t(std::clamp(mb,1,4096))*1024*1024/sizeof(Cluster));auto replacement=std::make_unique<Cluster[]>(n);table=std::move(replacement);size=n;generation=0;}
void TT::clear(){for(std::size_t i=0;i<size;++i)table[i]=Cluster{};generation=0;}
std::optional<TTEntry> TT::probe(U64 key) const {
    auto index=key&(size-1);std::lock_guard guard(locks[index&(locks.size()-1)]);
    for(auto e:table[index].entries)if(e.bound()!=NoBound&&e.key==key)return e;return {};
}
void TT::store(U64 key,Move move,int score,int eval,int depth,Bound bound,bool pv){
    auto index=key&(size-1);std::lock_guard guard(locks[index&(locks.size()-1)]);auto& row=table[index].entries;TTEntry* target=&row[0];
    auto quality=[&](const TTEntry& e){return int(e.depth)+4*e.pv()+4*(e.bound()==Exact)-8*((generation-e.generation())&31);};
    for(auto& e:row){if(e.bound()==NoBound){target=&e;break;}if(e.key==key){if(e.depth>depth+3&&bound!=Exact){if(move)e.move=move;return;}if(!move)move=e.move;target=&e;break;}if(quality(e)<quality(*target))target=&e;}
    *target={key,move,std::int16_t(score),std::int16_t(eval),std::uint8_t(std::clamp(depth,0,127)),std::uint8_t(bound|(pv?4:0)|(generation<<3))};
}
int TT::hashfull() const{int used=0;auto n=std::min<std::size_t>(250,size);for(std::size_t i=0;i<n;++i){std::lock_guard guard(locks[i&(locks.size()-1)]);for(auto e:table[i].entries)used+=e.bound()!=NoBound&&e.generation()==generation;}return int(used*1000/(n*4));}
}
