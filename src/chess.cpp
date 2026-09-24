#include "axiom/chess.hpp"
#include "axiom/hot_profile.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace axiom {
namespace {
constexpr int knight_dirs[]={33,31,18,14,-14,-18,-31,-33};
constexpr int bishop_dirs[]={17,15,-15,-17};
constexpr int rook_dirs[]={16,1,-1,-16};
constexpr int king_dirs[]={17,16,15,1,-1,-15,-16,-17};
std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
char symbol(int p) { char c=" pnbrqk"[std::abs(p)]; return p>0 ? static_cast<char>(std::toupper(c)) : c; }
const auto zobrist=[] { std::array<std::uint64_t,13000> values{}; for(std::size_t i=0;i<values.size();++i) values[i]=mix(i); return values; }();
bool attacked_on(const std::array<int,128>& cells,int sq,int by) {
    if(!valid(sq)) return false;
    for(int d:{by*15,by*17}) if(valid(sq-d) && cells[sq-d]==by*Pawn) return true;
    for(int d:knight_dirs) if(valid(sq+d) && cells[sq+d]==by*Knight) return true;
    for(int d:king_dirs) if(valid(sq+d) && cells[sq+d]==by*King) return true;
    for(int d:king_dirs) { bool diag=std::abs(d)==15 || std::abs(d)==17;
        for(int s=sq+d;valid(s);s+=d) { int p=cells[s]; if(!p) continue;
            if(p==by*Queen || p==by*(diag?Bishop:Rook)) return true; break; }
    } return false;
}
}
std::string square_name(int s) { return valid(s) ? std::string{char('a'+(s&7)),char('1'+(s>>4))} : "-"; }
int parse_square(const std::string& s) { return s.size()==2 && s[0]>='a' && s[0]<='h' && s[1]>='1' && s[1]<='8' ? (s[1]-'1')*16+s[0]-'a' : -1; }
std::string Move::uci() const { return !*this ? "0000" : square_name(from)+square_name(to)+(promotion ? std::string(1," pnbrqk"[promotion]) : ""); }
Board::Board() { set_fen(StartFen); }
Board::Board(const std::string& f) { set_fen(f); }
void Board::set_fen(const std::string& f) {
    std::istringstream in(f); std::string placement, turn, rights, eps, extra;
    int hm=0, fm=1;
    if (!(in>>placement>>turn>>rights>>eps>>hm>>fm) || (in>>extra) || hm<0 || hm>1000000 || fm<1 || fm>1000000)
        throw std::invalid_argument("FEN requires six valid fields");
    std::array<int,128> cells{}; int rank=7, file=0;
    for(char c:placement) {
        if(c=='/') { if(file!=8 || rank==0) throw std::invalid_argument("Invalid FEN rank"); --rank; file=0; }
        else if(c>='1' && c<='8') { file += c-'0'; if(file>8) throw std::invalid_argument("Invalid FEN width"); }
        else {
            const std::string names="pnbrqk"; auto i=names.find(static_cast<char>(std::tolower(c)));
            if(i==std::string::npos || file>=8) throw std::invalid_argument("Invalid FEN piece");
            int pt=static_cast<int>(i)+1;
            if(pt==Pawn && (rank==0 || rank==7)) throw std::invalid_argument("Pawn on back rank");
            cells[rank*16+file++] = (std::isupper(c)?1:-1)*pt;
        }
    }
    if(rank!=0 || file!=8 || (turn!="w" && turn!="b") ||
       std::count(cells.begin(),cells.end(),King)!=1 || std::count(cells.begin(),cells.end(),-King)!=1)
        throw std::invalid_argument("Invalid FEN placement or kings");
    int cr=0;
    for(char c:rights) { int bit=c=='K'?WK:c=='Q'?WQ:c=='k'?BK:c=='q'?BQ:0;
        if(c=='-' && rights=="-") continue;
        if(!bit || (cr&bit)) throw std::invalid_argument("Invalid castling rights"); cr|=bit;
    }
    int e=parse_square(eps), stm=turn=="w"?White:Black;
    if(eps!="-" && (!valid(e) || (e>>4)!=(stm==White?5:2) || cells[e]!=0 || cells[e-stm*16]!=-stm*Pawn))
        throw std::invalid_argument("Invalid en passant square");
    Board old=*this;
    squares=cells; side=stm; castle=cr; ep=e; halfmove=hm; fullmove=fm;
    if(attacked(king_square(-side),side)) { *this=old; throw std::invalid_argument("Side that just moved is in check"); }
    history.clear(); identities.clear(); history.push_back(hash()); identities.push_back(repetition_identity());
}
std::string Board::fen() const {
    std::ostringstream out;
    for(int r=7;r>=0;--r) { int empty=0;
        for(int f=0;f<8;++f) { int p=squares[r*16+f]; if(!p) ++empty;
            else { if(empty) out<<empty; empty=0; out<<symbol(p); } }
        if(empty) out<<empty; if(r) out<<'/';
    }
    out<<(side==White?" w ":" b ");
    if(!castle) out<<'-'; else { if(castle&WK) out<<'K'; if(castle&WQ) out<<'Q'; if(castle&BK) out<<'k'; if(castle&BQ) out<<'q'; }
    out<<' '<<square_name(ep)<<' '<<halfmove<<' '<<fullmove; return out.str();
}
int Board::king_square(int who) const { for(int s=0;s<128;++s) if(valid(s) && squares[s]==who*King) return s; return -1; }
bool Board::attacked(int sq,int by) const {
    AXIOM_HOT(Attack,Inherit);
    return attacked_on(squares,sq,by);
}
bool Board::piece_attacks(int from,int to) const {
    if(!valid(from) || !valid(to) || from==to || !squares[from]) return false;
    int pt=std::abs(squares[from]),c=color(squares[from]),diff=to-from;
    if(pt==Pawn) return diff==c*15 || diff==c*17;
    if(pt==Knight) return std::abs(diff)==14 || std::abs(diff)==18 || std::abs(diff)==31 || std::abs(diff)==33;
    int dx=(to&7)-(from&7),dy=(to>>4)-(from>>4);
    if(pt==King) return std::max(std::abs(dx),std::abs(dy))==1;
    bool diag=std::abs(dx)==std::abs(dy),straight=dx==0 || dy==0;
    if((pt==Bishop && !diag) || (pt==Rook && !straight) || (pt==Queen && !diag && !straight)) return false;
    int step=((dx>0)-(dx<0))+16*((dy>0)-(dy<0));
    for(int s=from+step;s!=to;s+=step) if(!valid(s) || squares[s]) return false;
    return true;
}
bool Board::legal_ep_exists() const {
    if(!valid(ep)) return false;
    for(int d:{side*15,side*17}) { int s=ep-d;
        if(valid(s) && squares[s]==side*Pawn) { auto cells=squares;
            cells[s]=0; cells[ep]=side*Pawn; cells[ep-side*16]=0;
            if(!attacked_on(cells,king_square(side),-side)) return true;
        }
    } return false;
}
std::uint64_t Board::hash() const {
    std::uint64_t h=zobrist[10000+castle] ^ (side==Black?zobrist[11000]:0);
    for(int s=0;s<128;++s) if(valid(s) && squares[s]) h^=zobrist[(squares[s]+6)*128+s];
    // Only legally available EP changes repetition identity (FIDE 9.2).
    if(legal_ep_exists()) h^=zobrist[12000+ep]; return h;
}
std::string Board::repetition_identity() const {
    std::string id; id.reserve(67);
    for(int s=0;s<128;++s) if(valid(s)) id+=static_cast<char>(squares[s]+6);
    id+=static_cast<char>(side+1); id+=static_cast<char>(castle);
    id+=static_cast<char>(legal_ep_exists()?ep:128); return id;
}
std::string Board::proof_key() const {
    AXIOM_HOT(ProofKey,Inherit);
    std::string key=repetition_identity(); key+=static_cast<char>(ep+1);
    for(int byte=0;byte<4;++byte) key+=static_cast<char>((halfmove>>(byte*8))&255);
    auto start=history.size()>static_cast<std::size_t>(halfmove+1)?history.size()-halfmove-1:0;
    for(std::size_t i=start;i<identities.size();++i) key+=identities[i];
    return key;
}
std::uint64_t Board::proof_hash() const {
    AXIOM_HOT(ProofKey,Inherit);
    // Compact search-context fingerprint. It covers the exact current repetition
    // identity, 50/75-move clock and all reversible-history position hashes.
    // The string form remains available for proof/debug paths; TT hot probes avoid
    // dynamic allocation and long byte comparisons.
    std::uint64_t h=mix(hash() ^ (std::uint64_t(halfmove)<<32) ^ std::uint64_t(ep+2));
    auto start=history.size()>static_cast<std::size_t>(halfmove+1)?history.size()-halfmove-1:0;
    for(std::size_t i=start;i<history.size();++i)
        h=mix(h ^ std::rotl(history[i],static_cast<int>((i-start)&63)) ^ (0x9e3779b97f4a7c15ULL+std::uint64_t(i-start)));
    return h;
}
std::vector<Move> Board::pseudo_moves() const {
    AXIOM_HOT(Pseudo,Inherit);
    std::vector<Move> moves; moves.reserve(64);
    auto add=[&](int s,int t) { if(std::abs(squares[s])==Pawn && ((t>>4)==0 || (t>>4)==7))
            for(int p:{Queen,Rook,Bishop,Knight}) moves.push_back({s,t,p});
        else moves.push_back({s,t,0}); };
    for(int s=0;s<128;++s) { if(!valid(s) || color(squares[s])!=side) continue;
        int pt=std::abs(squares[s]);
        if(pt==Pawn) { int t=s+side*16;
            if(valid(t) && !squares[t]) { add(s,t); int tt=t+side*16;
                if((s>>4)==(side==White?1:6) && !squares[tt]) add(s,tt); }
            for(int d:{side*15,side*17}) { t=s+d;
                if(valid(t) && ((color(squares[t])==-side && std::abs(squares[t])!=King) || (t==ep && squares[t-side*16]==-side*Pawn))) add(s,t); }
        } else {
            const int* dirs=pt==Knight?knight_dirs:pt==Bishop?bishop_dirs:pt==Rook?rook_dirs:king_dirs;
            int count=(pt==Bishop || pt==Rook)?4:8;
            for(int i=0;i<count;++i) for(int t=s+dirs[i];valid(t);t+=dirs[i]) {
                if(color(squares[t])==side || std::abs(squares[t])==King) break;
                add(s,t); if(squares[t] || pt==Knight || pt==King) break;
            }
        }
    }
    int home=side==White?0:112;
    if(squares[home+4]==side*King && !attacked(home+4,-side)) {
        if((castle&(side==White?WK:BK)) && squares[home+7]==side*Rook && !squares[home+5] && !squares[home+6] && !attacked(home+5,-side) && !attacked(home+6,-side)) add(home+4,home+6);
        if((castle&(side==White?WQ:BQ)) && squares[home]==side*Rook && !squares[home+1] && !squares[home+2] && !squares[home+3] && !attacked(home+3,-side) && !attacked(home+2,-side)) add(home+4,home+2);
    }
    return moves;
}
bool Board::capture(Move m) const { return squares[m.to]!=0 || (std::abs(squares[m.from])==Pawn && m.to==ep); }
Undo Board::push(Move m,bool record_history) {
    AXIOM_HOT(Push,Inherit); AXIOM_HOT_COPY(sizeof(squares));
    Undo u{squares,side,castle,ep,halfmove,fullmove,history.size()};
    int p=squares[m.from]; bool cap=capture(m); squares[m.from]=0;
    if(std::abs(p)==Pawn && m.to==ep && !squares[m.to]) squares[m.to-side*16]=0;
    squares[m.to]=m.promotion?side*m.promotion:p;
    if(std::abs(p)==King) {
        castle &= side==White?~(WK|WQ):~(BK|BQ);
        if(std::abs(m.to-m.from)==2) { int rf=m.to>m.from?m.from+3:m.from-4, rt=m.to>m.from?m.from+1:m.from-1;
            squares[rt]=squares[rf]; squares[rf]=0; }
    }
    for(int s:{m.from,m.to}) { if(s==0) castle&=~WQ; if(s==7) castle&=~WK; if(s==112) castle&=~BQ; if(s==119) castle&=~BK; }
    ep=std::abs(p)==Pawn && std::abs(m.to-m.from)==32?(m.from+m.to)/2:-1;
    halfmove=(cap || std::abs(p)==Pawn)?0:halfmove+1; if(side==Black) ++fullmove; side=-side;
    if(record_history) { history.push_back(hash()); identities.push_back(repetition_identity()); } return u;
}
Undo Board::push_null() { AXIOM_HOT(NullPush,Inherit); AXIOM_HOT_COPY(sizeof(squares)); Undo u{squares,side,castle,ep,halfmove,fullmove,history.size()}; side=-side; ep=-1; return u; }
void Board::pop(const Undo& u) { AXIOM_HOT(Pop,Inherit); AXIOM_HOT_COPY(sizeof(squares)); squares=u.squares; side=u.side; castle=u.castle; ep=u.ep; halfmove=u.halfmove; fullmove=u.fullmove; history.resize(u.history_size); identities.resize(u.history_size); }
#ifdef AXIOM_RESEARCH_TRACE
namespace {
// Observer-only classification: no instrumented attacked() query is added.
std::array<bool,128> audit_pins(const Board& b,int king) {
    std::array<bool,128> pins{};if(!active_hot_profile || !valid(king))return pins;
    for(int d:king_dirs){int blocker=-1;for(int s=king+d;valid(s);s+=d){
        if(!b.squares[s])continue;
        if(color(b.squares[s])==b.side){if(blocker!=-1)break;blocker=s;continue;}
        const int pt=std::abs(b.squares[s]);bool diag=std::abs(d)==15||std::abs(d)==17;
        if(blocker!=-1&&(pt==Queen||pt==(diag?Bishop:Rook)))pins[blocker]=true;break;
    }}return pins;
}
unsigned legal_classes(const Board& b,Move m,bool checked,bool pinned) {
    if(!active_hot_profile)return 0;
    const int pt=std::abs(b.squares[m.from]);
    unsigned mask=(1u<<(pt-1))|(1u<<(b.capture(m)?6:7))|(1u<<(checked?11:12))|(1u<<(pinned?13:14));
    if(m.promotion)mask|=1u<<8;if(pt==King&&std::abs(m.to-m.from)==2)mask|=1u<<9;
    if(pt==Pawn&&m.to==b.ep)mask|=1u<<10;return mask;
}
}
#endif
std::vector<Move> Board::legal_moves_reference() {
    AXIOM_HOT(Legal,Legal);
    auto moves=pseudo_moves(); std::vector<Move> legal; legal.reserve(moves.size());
    int king=king_square(side);
#ifdef AXIOM_RESEARCH_TRACE
    auto pins=audit_pins(*this,king);bool checked=active_hot_profile&&attacked_on(squares,king,-side);
#endif
    for(auto m:moves) {
#ifdef AXIOM_RESEARCH_TRACE
        LegalSample sample(legal_classes(*this,m,checked,pins[m.from]));
#endif
        bool kingMove=std::abs(squares[m.from])==King; auto u=push(m,false); bool ok=!attacked(kingMove?m.to:king,side); pop(u); if(ok) legal.push_back(m);
#ifdef AXIOM_RESEARCH_TRACE
        sample.finish(ok,true);
#endif
    } return legal;
}
std::vector<Move> Board::legal_moves() {
    if(!legal_fast_path) return legal_moves_reference();
    AXIOM_HOT(Legal,Legal);
    const int king=king_square(side);
    // Invalid/missing king and checked positions retain the complete oracle path.
    if(!valid(king) || attacked(king,-side)) return legal_moves_reference();
    std::array<bool,128> pinned{};
    for(int d:king_dirs) {
        int blocker=-1;
        for(int s=king+d;valid(s);s+=d) {
            if(!squares[s]) continue;
            if(color(squares[s])==side) {
                if(blocker!=-1) break;
                blocker=s; continue;
            }
            const int pt=std::abs(squares[s]);
            const bool diagonal=std::abs(d)==15 || std::abs(d)==17;
            if(blocker!=-1 && (pt==Queen || pt==(diagonal?Bishop:Rook))) pinned[blocker]=true;
            break;
        }
    }
    auto moves=pseudo_moves(); std::vector<Move> legal; legal.reserve(moves.size());
    for(auto m:moves) {
        const int pt=std::abs(squares[m.from]);
#ifdef AXIOM_RESEARCH_TRACE
        LegalSample sample(legal_classes(*this,m,false,pinned[m.from]));
#endif
        const bool epMove=pt==Pawn && m.to==ep;
        // Only moving the sole blocker of a slider ray can expose our stationary
        // king. Ordinary captures replace occupancy at destination. EP removes a
        // second square, so it is never covered by this proof. Promotion is safe.
        if(pt!=King && !pinned[m.from] && !epMove) {
            legal.push_back(m);
#ifdef AXIOM_RESEARCH_TRACE
            sample.finish(true,false);
#endif
            continue;
        }
        auto u=push(m,false); bool ok=!attacked(pt==King?m.to:king,side); pop(u);
        if(ok) legal.push_back(m);
#ifdef AXIOM_RESEARCH_TRACE
        sample.finish(ok,true);
#endif
    }
    return legal;
}
std::vector<Move> Board::legal_captures_to(int target) {
    AXIOM_HOT(LegalCaptures,Inherit);
    std::vector<Move> moves; moves.reserve(8);
    if(!valid(target) || color(squares[target])!=-side || std::abs(squares[target])==King) return moves;
    int king=king_square(side);
    for(int from=0;from<128;++from) if(valid(from) && color(squares[from])==side && piece_attacks(from,target)) {
        bool kingMove=std::abs(squares[from])==King;
        bool promotion=std::abs(squares[from])==Pawn && ((target>>4)==0 || (target>>4)==7);
        for(int p:promotion?std::initializer_list<int>{Queen,Rook,Bishop,Knight}:std::initializer_list<int>{0}) {
            Move m{from,target,p}; auto u=push(m,false); bool ok=!attacked(kingMove?target:king,side); pop(u); if(ok) moves.push_back(m);
        }
    } return moves;
}
bool Board::gives_check(Move m) { AXIOM_HOT(GivesCheck,GivesCheck); auto u=push(m,false); bool check=in_check(); pop(u); return check; }
std::optional<Move> Board::parse_move(const std::string& s) { for(auto m:legal_moves()) if(m.uci()==s) return m; return {}; }
int Board::repetitions() const { auto h=hash(); auto id=repetition_identity(); int n=0; auto start=history.size()>static_cast<std::size_t>(halfmove+1)?history.size()-halfmove-1:0;
    for(std::size_t i=start;i<history.size();++i) if(history[i]==h && identities[i]==id) ++n; return n; }
bool Board::insufficient() const {
    int minors=0, knights=0, bishop_color=-1; bool mixed=false;
    for(int s=0;s<128;++s) if(valid(s) && squares[s]) { int p=std::abs(squares[s]);
        if(p==Pawn || p==Rook || p==Queen) return false;
        if(p==Knight) { ++minors; ++knights; }
        if(p==Bishop) { ++minors; int c=((s&7)+(s>>4))&1; if(bishop_color>=0 && bishop_color!=c) mixed=true; bishop_color=c; }
    }
    return minors<=1 || (knights==0 && !mixed);
}
bool Board::automatic_draw() const { return halfmove>=150 || (halfmove>=16 && repetitions()>=5) || insufficient(); }
bool Board::claim_now() const { return halfmove>=100 || (halfmove>=8 && repetitions()>=3); }
bool Board::can_claim_draw(bool legal_known) {
    // The earliest intended third repetition is after seven reversible plies.
    if(halfmove<7) return false;
    std::vector<Move> moves;
    if(!legal_known) { moves=legal_moves(); if(moves.empty()) return false; }
    if(claim_now()) return true;
    if(halfmove<99 && history.size()<8) return false;
    if(moves.empty()) moves=legal_moves();
    for(auto m:moves) { auto u=push(m); bool claim=claim_now();
        if(claim && in_check() && legal_moves().empty()) claim=false;
        pop(u); if(claim) return true; } return false;
}
int Board::piece_count() const { int n=0; for(int s=0;s<128;++s) if(valid(s) && squares[s]) ++n; return n; }
std::uint64_t perft(Board& b,int depth) { if(depth==0) return 1; auto moves=b.legal_moves(); if(depth==1) return moves.size(); std::uint64_t n=0;
    for(auto m:moves) { auto u=b.push(m); n+=perft(b,depth-1); b.pop(u); } return n; }
}
