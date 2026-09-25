#include "position.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>
namespace ax2 {
namespace {
struct Keys {U64 piece[16][64]{},castle[16]{},ep[8]{},side; Keys(){U64 n=1;for(auto& row:piece)for(auto& v:row)v=mix(n++);for(auto& v:castle)v=mix(n++);for(auto& v:ep)v=mix(n++);side=mix(n);}};
const Keys keys;
}
Position::Position(){set_fen(StartFen);} Position::Position(const std::string& f){set_fen(f);}
void Position::put(int s,int pc) {
    assert(pc&& !board[s]); board[s]=std::uint8_t(pc);int c=side_of(pc),pt=type(pc);
    bb[c][pt]|=bit(s);occupied[c]|=bit(s);key^=keys.piece[pc][s];
    mg+=pst(pc,s,false);eg+=pst(pc,s,true);phase+=phaseWeight[pt];
}
void Position::remove(int s) {
    int pc=board[s];assert(pc);int c=side_of(pc),pt=type(pc);
    bb[c][pt]^=bit(s);occupied[c]^=bit(s);key^=keys.piece[pc][s];board[s]=0;
    mg-=pst(pc,s,false);eg-=pst(pc,s,true);phase-=phaseWeight[pt];
}
U64 Position::attackers(int s,int c,U64 occ,U64 removed) const {
    return ((attacks.pawn[c^1][s]&bb[c][Pawn])|(attacks.knight[s]&bb[c][Knight])|
        (attacks.king[s]&bb[c][King])|(bishop_attacks(s,occ)&(bb[c][Bishop]|bb[c][Queen]))|
        (rook_attacks(s,occ)&(bb[c][Rook]|bb[c][Queen])))&~removed;
}
int Position::canonical_ep() const {
    if(ep<0)return -1;
    U64 candidates=attacks.pawn[side^1][ep]&bb[side][Pawn];int captured=ep+(side?8:-8);
    while(candidates) {int s=pop(candidates);U64 occ=(occupancy()&~bit(s)&~bit(captured))|bit(ep);
        if(!attackers(king(side),side^1,occ,bit(captured)))return file(ep);
    }return -1;
}
void Position::set_fen(const std::string& f) {
    // Parse into a temporary so malformed input never corrupts the live game.
    std::istringstream in(f);std::string cells,turn,rights,eps,extra;int hm,fm;
    if(!(in>>cells>>turn>>rights>>eps>>hm>>fm)||(in>>extra)||hm<0||hm>1000000||fm<1||fm>1000000)throw std::invalid_argument("invalid FEN fields");
    auto old=*this;board={};bb={};occupied={};key=0;mg=eg=phase=0;
    try {
        int r=7,fl=0;
        for(char c:cells) {if(c=='/'){if(fl!=8||r==0)throw std::invalid_argument("FEN rank");--r;fl=0;}
            else if(c>='1'&&c<='8'){fl+=c-'0';if(fl>8)throw std::invalid_argument("FEN width");}
            else {auto pt=std::string(" pnbrqk").find(char(std::tolower(static_cast<unsigned char>(c))));
                if(pt==std::string::npos||pt==0||fl>=8||(pt==Pawn&&(r==0||r==7)))throw std::invalid_argument("FEN piece");
                put(r*8+fl++,piece(std::isupper(static_cast<unsigned char>(c))?White:Black,int(pt)));}}
        if(r!=0||fl!=8||count(bb[0][King])!=1||count(bb[1][King])!=1||(turn!="w"&&turn!="b")||count(occupancy())>32)throw std::invalid_argument("FEN placement");
        side=turn=="b";castle=0;
        for(char c:rights){int v=c=='K'?1:c=='Q'?2:c=='k'?4:c=='q'?8:0;if(c=='-'&&rights=="-")continue;if(!v||(castle&v))throw std::invalid_argument("FEN castling");castle|=v;}
        ep=parse_square(eps);if(eps!="-"&&(ep<0||rank(ep)!=(side?2:5)||board[ep]||board[ep+(side?8:-8)]!=piece(side^1,Pawn)))throw std::invalid_argument("FEN EP");
        halfmove=hm;fullmove=fm;
        if(attacked(king(side^1),side))throw std::invalid_argument("FEN previous king in check");
        key=recompute_key();context=mix(key);historySize=1;history[0]=key;nullBoundary=0;repetitionPair=false;
    }catch(...){*this=old;throw;}
}
std::string Position::fen() const {
    std::ostringstream out;
    for(int r=7;r>=0;--r){int empty=0;for(int f=0;f<8;++f){int pc=board[r*8+f];if(!pc)++empty;else{if(empty)out<<empty;empty=0;char c=" pnbrqk"[type(pc)];out<<char(side_of(pc)?c:std::toupper(c));}}if(empty)out<<empty;if(r)out<<'/';}
    out<<(side?" b ":" w ");if(!castle)out<<'-';else{if(castle&1)out<<'K';if(castle&2)out<<'Q';if(castle&4)out<<'k';if(castle&8)out<<'q';}
    out<<' '<<(ep<0?"-":square(ep))<<' '<<halfmove<<' '<<fullmove;return out.str();
}
void Position::save(Undo& u) const {u.key=key;u.context=context;u.castle=castle;u.ep=ep;u.halfmove=halfmove;u.fullmove=fullmove;u.historySize=historySize;u.nullBoundary=nullBoundary;u.repetitionPair=repetitionPair;}
void Position::make(Move m,Undo& u) {
    if(historySize>=int(history.size()))throw std::length_error("position history capacity");save(u);int from=m.from(),to=m.to(),pc=board[from],pt=type(pc);u.moved=pc;u.capturedSquare=to;
    int oldEp=canonical_ep();if(oldEp>=0)key^=keys.ep[oldEp];key^=keys.castle[castle];
    if(pt==Pawn&&to==ep&&!board[to])u.capturedSquare=to+(side?8:-8);
    u.captured=board[u.capturedSquare];if(u.captured)remove(u.capturedSquare);remove(from);put(to,m.promo()?piece(side,m.promo()):pc);
    if(pt==King){castle&=side?3:12;if(std::abs(to-from)==2){int rf=to>from?from+3:from-4,rt=to>from?from+1:from-1;remove(rf);put(rt,piece(side,Rook));}}
    for(int s:{from,to}) {if(s==0)castle&=~2;if(s==7)castle&=~1;if(s==56)castle&=~8;if(s==63)castle&=~4;}
    ep=pt==Pawn&&std::abs(to-from)==16?(from+to)/2:-1;halfmove=pt==Pawn||u.captured?0:halfmove+1;if(side)++fullmove;side^=1;
    key^=keys.side^keys.castle[castle];int newEp=canonical_ep();if(newEp>=0)key^=keys.ep[newEp];
    context=halfmove==0||castle!=u.castle?mix(key):std::rotl(context,7)^mix(key);
    history[historySize++]=key;
    if(!halfmove||castle!=u.castle)repetitionPair=false;
    else if(!nullBoundary&&repetitions()>=2)repetitionPair=true;
}
void Position::unmake(Move m,const Undo& u) {
    side^=1;remove(m.to());put(m.from(),u.moved);
    if(type(u.moved)==King&&std::abs(m.to()-m.from())==2){int rf=m.to()>m.from()?m.from()+3:m.from()-4,rt=m.to()>m.from()?m.from()+1:m.from()-1;remove(rt);put(rf,piece(side,Rook));}
    if(u.captured)put(u.capturedSquare,u.captured);
    key=u.key;context=u.context;castle=u.castle;ep=u.ep;halfmove=u.halfmove;fullmove=u.fullmove;historySize=u.historySize;nullBoundary=u.nullBoundary;repetitionPair=u.repetitionPair;
}
void Position::make_null(Undo& u){save(u);int e=canonical_ep();if(e>=0)key^=keys.ep[e];key^=keys.side;side^=1;ep=-1;nullBoundary=historySize;context=mix(key);}
void Position::unmake_null(const Undo& u){side^=1;key=u.key;context=u.context;ep=u.ep;nullBoundary=u.nullBoundary;}
U64 Position::recompute_key() const {U64 h=keys.castle[castle]^(side?keys.side:0);for(int s=0;s<64;++s)if(board[s])h^=keys.piece[board[s]][s];int e=canonical_ep();if(e>=0)h^=keys.ep[e];return h;}
bool Position::consistent() const {
    U64 b[2][7]{},o[2]{};int m=0,e=0,ph=0;
    for(int s=0;s<64;++s)if(int pc=board[s]){b[side_of(pc)][type(pc)]|=bit(s);o[side_of(pc)]|=bit(s);m+=pst(pc,s,false);e+=pst(pc,s,true);ph+=phaseWeight[type(pc)];}
    for(int c=0;c<2;++c){if(o[c]!=occupied[c])return false;for(int t=1;t<=6;++t)if(b[c][t]!=bb[c][t])return false;}
    return key==recompute_key()&&m==mg&&e==eg&&ph==phase;
}
int Position::repetitions() const {if(nullBoundary)return 0;int n=0;for(int i=historySize-1;i>=std::max(0,historySize-1-halfmove);i-=2)if(history[i]==key)++n;return n;}
bool Position::can_claim(const MoveList& legal){
    if(nullBoundary||!legal.size)return false;
    if(claim_now())return true;
    if(halfmove<99&&!repetitionPair)return false;
    for(auto m:legal){Undo u;make(m,u);bool claim=claim_now();if(claim&&in_check()&&!legal_moves().size)claim=false;unmake(m,u);if(claim)return true;}
    return false;
}
bool Position::insufficient() const {
    if(bb[0][Pawn]|bb[1][Pawn]|bb[0][Rook]|bb[1][Rook]|bb[0][Queen]|bb[1][Queen])return false;
    if(count(occupancy())<=3)return true;
    if(bb[0][Knight]|bb[1][Knight])return false;
    U64 b=bb[0][Bishop]|bb[1][Bishop],dark=0xAA55AA55AA55AA55ULL;return !(b&dark)||!(b&~dark);
}
Move Position::parse_move(const std::string& text) const {for(auto m:legal_moves())if(m.uci()==text)return m;return {};}
std::uint64_t perft(Position& p,int d){if(!d)return 1;auto moves=p.legal_moves();if(d==1)return moves.size;U64 n=0;for(auto m:moves){Undo u;p.make(m,u);n+=perft(p,d-1);p.unmake(m,u);}return n;}
}
