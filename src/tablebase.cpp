#include "axiom/engine.hpp"
#ifdef AXIOM_FATHOM
#include <tbprobe.h>
#endif
namespace axiom {
bool Tablebase::compiled() {
#ifdef AXIOM_FATHOM
    return true;
#else
    return false;
#endif
}
bool Tablebase::open(const std::string& path) {
#ifdef AXIOM_FATHOM
    ready_=tb_init(path.c_str()) && TB_LARGEST>0;
#else
    (void)path; ready_=false;
#endif
    return ready_;
}
std::optional<TBResult> Tablebase::probe(Board& b) const {
#ifdef AXIOM_FATHOM
    if(!ready_ || b.castle || b.piece_count()>static_cast<int>(TB_LARGEST)) return {};
    std::uint64_t white=0,black=0,pieces[7]{};
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]) { int p=b.squares[s]; auto bit=1ULL<<((s>>4)*8+(s&7));
        (p>0?white:black)|=bit; pieces[std::abs(p)]|=bit; }
    unsigned ep=valid(b.ep)?(b.ep>>4)*8+(b.ep&7):0;
    unsigned r=tb_probe_root(white,black,pieces[King],pieces[Queen],pieces[Rook],pieces[Bishop],pieces[Knight],pieces[Pawn],b.halfmove,b.castle,ep,b.side==White,nullptr);
    if(r==TB_RESULT_FAILED || r==TB_RESULT_CHECKMATE || r==TB_RESULT_STALEMATE) return {};
    int from=TB_GET_FROM(r), to=TB_GET_TO(r); constexpr int promos[]={0,Queen,Rook,Bishop,Knight};
    unsigned promotion=TB_GET_PROMOTES(r); if(promotion>4) return {};
    Move m{(from/8)*16+from%8,(to/8)*16+to%8,promos[promotion]};
    if(!b.parse_move(m.uci())) return {};
    TBResult result; result.wdl=static_cast<int>(TB_GET_WDL(r))-2;
    result.dtz=static_cast<int>(TB_GET_DTZ(r))*(result.wdl<0?-1:1); result.best=m;
    // Root DTZ cannot encode the caller's repetition history. Conservatively
    // certify only a fresh zeroing state; otherwise expose advisory data.
    result.exact=b.halfmove==0;
    result.reason=result.exact?"Syzygy WDL/DTZ at zero halfmove clock":"Advisory: repetition/50-move history not certified";
    return result;
#else
    (void)b; return {};
#endif
}
}
