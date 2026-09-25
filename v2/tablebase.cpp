#include "tablebase.hpp"
#ifdef AXIOM_FATHOM
#include <tbprobe.h>
#endif
namespace ax2 {
bool Tablebase::compiled(){
#ifdef AXIOM_FATHOM
    return true;
#else
    return false;
#endif
}
bool Tablebase::open(const std::string& path){
#ifdef AXIOM_FATHOM
    ready=tb_init(path.c_str())&&TB_LARGEST>0;
#else
    (void)path;ready=false;
#endif
    return ready;
}
std::optional<int> Tablebase::wdl(const Position& p) const {
#ifdef AXIOM_FATHOM
    // WDL probing requires a fresh zeroing state: nonzero rule50/repetition is not certified.
    if(!ready||p.castle||p.halfmove||p.nullBoundary||count(p.occupancy())>int(TB_LARGEST))return {};
    const auto& b=p.bb;
    unsigned r=tb_probe_wdl(p.occupied[0],p.occupied[1],b[0][King]|b[1][King],b[0][Queen]|b[1][Queen],b[0][Rook]|b[1][Rook],b[0][Bishop]|b[1][Bishop],b[0][Knight]|b[1][Knight],b[0][Pawn]|b[1][Pawn],0,0,p.ep<0?0:p.ep,p.side==White);
    if(r==TB_RESULT_FAILED)return {};return r==TB_WIN?20000:r==TB_LOSS?-20000:0;
#else
    (void)p;return {};
#endif
}
std::optional<TBRoot> Tablebase::root(const Position& p) const {
#ifdef AXIOM_FATHOM
    if(!ready||p.castle||p.nullBoundary||count(p.occupancy())>int(TB_LARGEST))return {};
    const auto& b=p.bb;
    unsigned r=tb_probe_root(p.occupied[0],p.occupied[1],b[0][King]|b[1][King],b[0][Queen]|b[1][Queen],b[0][Rook]|b[1][Rook],b[0][Bishop]|b[1][Bishop],b[0][Knight]|b[1][Knight],b[0][Pawn]|b[1][Pawn],p.halfmove,0,p.ep<0?0:p.ep,p.side==White,nullptr);
    if(r==TB_RESULT_FAILED||r==TB_RESULT_CHECKMATE||r==TB_RESULT_STALEMATE)return {};
    constexpr int promos[]={0,Queen,Rook,Bishop,Knight};unsigned pr=TB_GET_PROMOTES(r);if(pr>4)return {};
    Move m(TB_GET_FROM(r),TB_GET_TO(r),promos[pr]);if(!p.parse_move(m.uci()))return {};
    int w=int(TB_GET_WDL(r))-2;return TBRoot{m,w==2?20000:w==-2?-20000:0,w,int(TB_GET_DTZ(r))};
#else
    (void)p;return {};
#endif
}
}
