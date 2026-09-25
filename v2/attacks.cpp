#include "attacks.hpp"
#include <cmath>
namespace ax2 {
U64 mix(U64 x) { x+=0x9e3779b97f4a7c15ULL; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL; x=(x^(x>>27))*0x94d049bb133111ebULL; return x^(x>>31); }
std::string square(int s) {return std::string{char('a'+file(s)),char('1'+rank(s))};}
int parse_square(const std::string& s) {return s.size()==2&&s[0]>='a'&&s[0]<='h'&&s[1]>='1'&&s[1]<='8'?(s[1]-'1')*8+s[0]-'a':-1;}
std::string Move::uci() const {return !*this?"0000":square(from())+square(to())+(promo()?std::string(1," pnbrqk"[promo()]):"");}
namespace {
U64 ray(int s,U64 occ,int df,int dr) {
    U64 b=0;
    for(int f=file(s)+df,r=rank(s)+dr;f>=0&&f<8&&r>=0&&r<8;f+=df,r+=dr) {
        U64 v=bit(r*8+f); b|=v; if(occ&v)break;
    } return b;
}
}
U64 bishop_attacks(int s,U64 o){return ray(s,o,1,1)|ray(s,o,-1,1)|ray(s,o,1,-1)|ray(s,o,-1,-1);}
U64 rook_attacks(int s,U64 o){return ray(s,o,1,0)|ray(s,o,-1,0)|ray(s,o,0,1)|ray(s,o,0,-1);}
AttackTables::AttackTables() {
    for(int s=0;s<64;++s) for(int t=0;t<64;++t) {
        int dx=file(t)-file(s),dy=rank(t)-rank(s);
        if(std::abs(dx)*std::abs(dy)==2)knight[s]|=bit(t);
        if(s!=t&&std::max(std::abs(dx),std::abs(dy))==1)king[s]|=bit(t);
        for(int c=0;c<2;++c)if(std::abs(dx)==1&&dy==(c?-1:1))pawn[c][s]|=bit(t);
        if(s==t || !(dx==0||dy==0||std::abs(dx)==std::abs(dy)))continue;
        int df=(dx>0)-(dx<0),dr=(dy>0)-(dy<0),step=df+8*dr;
        for(int k=s+step;k!=t;k+=step)between[s][t]|=bit(k);
        line[s][t]=bit(s)|ray(s,0,df,dr)|ray(s,0,-df,-dr);
    }
}
const AttackTables attacks;
}
