#include "evaluation.hpp"
namespace ax2 {
bool see_ge(const Position& p,Move m,int threshold) {
    // Local bitboards only: no Position copy, legal move generation or allocation.
    if(type(p.board[m.from()])==King&&std::abs(m.to()-m.from())==2)return 0>=threshold;
    int gain[32]{},n=0,us=p.side,to=m.to(),moving=type(p.board[m.from()]);
    gain[0]=values[p.victim(m)]+(m.promo()?values[m.promo()]-values[Pawn]:0);
    if(gain[0]<threshold)return false; // Opponent can always decline an exchange.
    auto bb=p.bb;U64 occ=p.occupancy();int cap=to;
    if(moving==Pawn&&to==p.ep&&!p.board[to])cap=to+(us?8:-8);
    if(p.board[cap])bb[us^1][type(p.board[cap])]&=~bit(cap);
    bb[us][moving]&=~bit(m.from());occ&=~bit(m.from());occ&=~bit(cap);occ|=bit(to);
    int occupant=m.promo()?m.promo():moving,occupantSide=us;bb[us][occupant]|=bit(to);us^=1;
    auto attackers=[&](int s,int c,U64 o){return (attacks.pawn[c^1][s]&bb[c][Pawn])|(attacks.knight[s]&bb[c][Knight])|
        (attacks.king[s]&bb[c][King])|(bishop_attacks(s,o)&(bb[c][Bishop]|bb[c][Queen]))|(rook_attacks(s,o)&(bb[c][Rook]|bb[c][Queen]));};
    while(n<30){U64 candidates=attackers(to,us,occ);int from=-1,pt=0,newPt=0;
        // Ignore pinned and illegal king recaptures using the evolving occupancy.
        for(int t=Pawn;t<=King&&from<0;++t){U64 options=candidates&bb[us][t];while(options){int s=pop(options);int promoted=t==Pawn&&(rank(to)==0||rank(to)==7)?Queen:t;
            bb[occupantSide][occupant]&=~bit(to);bb[us][t]&=~bit(s);bb[us][promoted]|=bit(to);
            U64 nextOcc=occ&~bit(s);int k=std::countr_zero(bb[us][King]);bool legal=!attackers(k,us^1,nextOcc);
            bb[us][promoted]&=~bit(to);bb[us][t]|=bit(s);bb[occupantSide][occupant]|=bit(to);
            if(legal){from=s;pt=t;newPt=promoted;break;}
        }}
        if(from<0)break;
        ++n;gain[n]=values[occupant]+(newPt!=pt?values[newPt]-values[pt]:0)-gain[n-1];
        bb[occupantSide][occupant]&=~bit(to);bb[us][pt]&=~bit(from);bb[us][newPt]|=bit(to);occ&=~bit(from);
        occupant=newPt;occupantSide=us;us^=1;
    }
    while(n>0){gain[n-1]=-std::max(-gain[n-1],gain[n]);--n;}
    return gain[0]>=threshold;
}
}
