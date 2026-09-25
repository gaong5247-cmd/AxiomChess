#include "position.hpp"
namespace ax2 {
MoveList Position::legal_moves() const {
    MoveList moves;int us=side,them=us^1,k=king(us);U64 occ=occupancy(),own=occupied[us],enemy=occupied[them]&~bb[them][King];
    U64 checkers=attackers(k,them,occ),allowed=~U64(0),pinned=0;
    int checks=count(checkers);
    if(checks==1){int s=std::countr_zero(checkers);allowed=bit(s)|attacks.between[k][s];}
    U64 sliders=(bishop_attacks(k,0)&(bb[them][Bishop]|bb[them][Queen]))|(rook_attacks(k,0)&(bb[them][Rook]|bb[them][Queen]));
    while(sliders){int s=pop(sliders);U64 blockers=attacks.between[k][s]&occ;if(count(blockers)==1&&(blockers&own))pinned|=blockers;}
    auto add=[&](int s,int t){if(type(board[s])==Pawn&&(rank(t)==0||rank(t)==7))for(int pt:{Queen,Rook,Bishop,Knight})moves.add(Move(s,t,pt));else moves.add(Move(s,t));};
    U64 targets=attacks.king[k]&~own&~bb[them][King];
    while(targets){int t=pop(targets);if(!attackers(t,them,(occ&~bit(k))|bit(t),bit(t)))add(k,t);}
    if(checks>=2)return moves;
    U64 pawns=bb[us][Pawn];int step=us?-8:8;
    while(pawns){int s=pop(pawns);U64 mask=allowed&((pinned&bit(s))?attacks.line[k][s]:~U64(0));int t=s+step;
        if(t>=0&&t<64&&!board[t]){if(mask&bit(t))add(s,t);int tt=t+step;if(rank(s)==(us?6:1)&&!board[tt]&&(mask&bit(tt)))add(s,tt);}
        U64 caps=attacks.pawn[us][s]&enemy&mask;while(caps)add(s,pop(caps));
        if(ep>=0&&(attacks.pawn[us][s]&bit(ep))){int cap=ep-step;U64 after=(occ&~bit(s)&~bit(cap))|bit(ep);
            if(!attackers(k,them,after,bit(cap)))add(s,ep);}
    }
    for(int pt=Knight;pt<=Queen;++pt){U64 pieces=bb[us][pt];while(pieces){int s=pop(pieces);U64 dest=piece_attacks(pt,s,occ)&~own&~bb[them][King]&allowed;
        if(pinned&bit(s))dest&=attacks.line[k][s];while(dest)add(s,pop(dest));}}
    int base=us?56:0;
    if(!checkers&&k==base+4){
        if((castle&(us?4:1))&&board[base+7]==piece(us,Rook)&&!(occ&(bit(base+5)|bit(base+6)))&&
           !attackers(base+5,them,occ&~bit(k))&&!attackers(base+6,them,occ&~bit(k)))add(k,base+6);
        if((castle&(us?8:2))&&board[base]==piece(us,Rook)&&!(occ&(bit(base+1)|bit(base+2)|bit(base+3)))&&
           !attackers(base+3,them,occ&~bit(k))&&!attackers(base+2,them,occ&~bit(k)))add(k,base+2);
    }
    return moves;
}
}
