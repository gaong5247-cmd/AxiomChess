#include "evaluation.hpp"
#include <cmath>
namespace ax2 {
int pst(int pc,int s,bool endgame) {
    int c=side_of(pc),pt=type(pc),r=c?7-rank(s):rank(s),f=file(s);
    int center=14-std::abs(2*f-7)-std::abs(2*rank(s)-7),v=pt==King?0:values[pt];
    switch(pt){
    case Pawn:v+=r*(endgame?9:5)+center*2;break;
    case Knight:v+=center*(endgame?4:6)-25*(r==0);break;
    case Bishop:v+=center*3+5*r;break;
    case Rook:v+=r*3+18*(r==6);break;
    case Queen:v+=center*(endgame?2:1)-8*(r>2&&!endgame);break;
    case King:v+=endgame?center*6:-r*15-2*center+25*(r==0&&(f==6||f==2));break;
    }return c?-v:v;
}
int evaluate(const Position& p) {
    int mg=p.mg,eg=p.eg,phase=std::min(24,p.phase);U64 occ=p.occupancy();
    for(int c=0;c<2;++c){int sign=c?-1:1;U64 pawns=p.bb[c][Pawn],enemyPawns=p.bb[c^1][Pawn],pawnAttacks=0,enemyAttacks=0;
        U64 it=pawns;while(it)pawnAttacks|=attacks.pawn[c][pop(it)];it=enemyPawns;while(it)enemyAttacks|=attacks.pawn[c^1][pop(it)];
        it=pawns;while(it){int s=pop(it),f=file(s),r=c?7-rank(s):rank(s);U64 adjacent=(f?file_mask(f-1):0)|(f<7?file_mask(f+1):0);
            int penalty=12*(!(pawns&adjacent))+10*(count(pawns&file_mask(f))>1);mg-=sign*penalty;eg-=sign*penalty;
            U64 ahead=0;for(int rr=rank(s)+(c?-1:1);rr>=0&&rr<8;rr+=c?-1:1)ahead|=U64(255)<<(rr*8);
            if(!(enemyPawns&ahead&(adjacent|file_mask(f)))){mg+=sign*r*r*2;eg+=sign*r*r*5;}
            mg+=sign*std::max(0,r-2)*2;
        }
        if(count(p.bb[c][Bishop])>=2){mg+=sign*28;eg+=sign*40;}
        U64 ring=attacks.king[p.king(c^1)]|p.bb[c^1][King];int pressure=0,attackers=0;
        for(int pt=Knight;pt<=Queen;++pt){it=p.bb[c][pt];while(it){int s=pop(it);U64 a=piece_attacks(pt,s,occ),safe=a&~p.occupied[c]&~enemyAttacks;int mobility=count(safe);
            mg+=sign*mobility*(pt==Queen?1:3);eg+=sign*mobility*2;
            if(a&ring){pressure+=count(a&ring)*(pt==Queen?5:pt==Rook?3:2);++attackers;}
            U64 weak=a&p.occupied[c^1]&~enemyAttacks&~p.bb[c^1][King];mg+=sign*count(weak)*5;
            if(pt==Rook&&!(pawns&file_mask(file(s)))){mg+=sign*((enemyPawns&file_mask(file(s)))?12:22);eg+=sign*10;}
            if((pt==Knight||pt==Bishop)&&(pawnAttacks&bit(s))&&!(enemyAttacks&bit(s))){int r=c?7-rank(s):rank(s);if(r>=3&&r<=5)mg+=sign*18;}
        }}
        mg+=sign*pressure*std::min(4,attackers);
        int k=p.king(c),shield=0;U64 front=attacks.pawn[c][k];shield=count(front&pawns);mg+=sign*shield*12;
    }
    int score=(mg*phase+eg*(24-phase))/24;
    if(!(p.bb[0][Pawn]|p.bb[1][Pawn])&&std::abs(score)<450)score/=2;
    return std::clamp((p.side?-score:score)+10,-MateBound+1,MateBound-1);
}
}
