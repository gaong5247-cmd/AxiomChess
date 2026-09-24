#include "axiom/engine.hpp"
#include <algorithm>
#include <cmath>

namespace axiom {
int piece_value(int p) { constexpr int v[]={0,100,320,335,500,950,20000}; return v[std::abs(p)]; }
Evaluation evaluate(const Board& b) {
    Evaluation e;
    for(auto name:{"Material","Mobility","KingSafety","PawnStructure","PieceActivity","Space","Threats","PassedPawns","EndgameTerms"}) e.terms[name]=0;
    int phase=0; constexpr int pw[]={0,0,1,1,2,4,0};
    for(int s=0;s<128;++s) if(valid(s)) phase+=pw[std::abs(b.squares[s])];
    phase=std::min(24,phase); e.phase=phase/24.0;
    int files[2][8]{}; int bishops[2]{};
    for(int s=0;s<128;++s) if(valid(s)) { int p=b.squares[s], c=p>0?0:1;
        if(std::abs(p)==Pawn) ++files[c][s&7]; if(std::abs(p)==Bishop) ++bishops[c]; }
    for(int s=0;s<128;++s) {
        if(!valid(s) || !b.squares[s]) continue;
        int p=b.squares[s], c=color(p), pt=std::abs(p), sign=c*b.side, ci=c==White?0:1;
        int rank=c==White?s>>4:7-(s>>4), file=s&7;
        int central=14-std::abs(2*file-7)-std::abs(2*(s>>4)-7);
        if(pt!=King) {
            e.terms["Material"]+=sign*piece_value(pt);
            e.terms["PieceActivity"]+=sign*central*(pt==Knight || pt==Bishop?3:1);
            if(b.attacked(s,-c)) e.terms["Threats"]-=sign*piece_value(pt)/(b.attacked(s,c)?40:8);
        }
        if(pt==Pawn) {
            bool isolated=(file==0 || files[ci][file-1]==0) && (file==7 || files[ci][file+1]==0);
            e.terms["PawnStructure"]-=sign*(12*isolated+8*(files[ci][file]>1));
            bool passed=true;
            for(int f=std::max(0,file-1);f<=std::min(7,file+1);++f)
                for(int r=(s>>4)+c;r>=0 && r<8;r+=c) if(b.squares[r*16+f]==-c*Pawn) passed=false;
            if(passed) e.terms["PassedPawns"]+=sign*rank*rank*(120-3*phase)/24;
            e.terms["Space"]+=sign*std::max(0,rank-2)*phase/8;
        } else if(pt==King) {
            int danger=0;
            for(int d:{0,1,-1,16,-16,15,-15,17,-17}) if(valid(s+d) && b.attacked(s+d,-c)) ++danger;
            e.terms["KingSafety"]-=sign*danger*12*phase/24;
            e.terms["EndgameTerms"]+=sign*central*4*(24-phase)/24;
        } else {
            constexpr int nd[]={33,31,18,14,-14,-18,-31,-33};
            constexpr int sd[]={17,16,15,1,-1,-15,-16,-17};
            const int* dirs=pt==Knight?nd:sd; int mobility=0;
            for(int i=0;i<8;++i) { int d=dirs[i]; bool diagonal=std::abs(d)==15 || std::abs(d)==17;
                if(pt==Bishop && !diagonal) continue; if(pt==Rook && diagonal) continue;
                for(int t=s+d;valid(t);t+=d) { if(color(b.squares[t])==c) break; ++mobility;
                    if(b.squares[t] || pt==Knight) break; }
            } e.terms["Mobility"]+=sign*mobility*3;
        }
    }
    for(int ci=0;ci<2;++ci) if(bishops[ci]>=2) e.terms["PieceActivity"]+=(ci==0?1:-1)*b.side*25;
    for(const auto& [name,value]:e.terms) { (void)name; e.total+=value; }
    e.total=std::clamp(e.total,-MateThreshold+1,MateThreshold-1); return e;
}
namespace {
int capture_gain(const Board& b,Move m) {
    int v=piece_value(b.squares[m.to]);
    if(std::abs(b.squares[m.from])==Pawn && m.to==b.ep && !b.squares[m.to]) v=100;
    if(m.promotion) v+=piece_value(m.promotion)-100;
    return v;
}

struct SeeAttacker { int square=-1,piece=0; };

bool ray_attacks(const std::array<int,128>& cells,int from,int target,int piece) {
    int dx=(target&7)-(from&7),dy=(target>>4)-(from>>4);
    bool diag=std::abs(dx)==std::abs(dy),straight=dx==0 || dy==0;
    if((piece==Bishop && !diag) || (piece==Rook && !straight) || (piece==Queen && !diag && !straight)) return false;
    int step=((dx>0)-(dx<0))+16*((dy>0)-(dy<0));
    if(!step) return false;
    for(int s=from+step;s!=target;s+=step) if(!valid(s) || cells[s]) return false;
    return true;
}

SeeAttacker least_attacker(const std::array<int,128>& cells,int target,int side) {
    // LVA order. Kings are deliberately omitted: this makes SEE conservative
    // around king recaptures rather than risking an illegal king capture prune.
    for(int pt:{Pawn,Knight,Bishop,Rook,Queen}) {
        int best=-1;
        for(int s=0;s<128;++s) if(valid(s) && cells[s]==side*pt) {
            bool attacks=false;
            if(pt==Pawn) {
                const int diff=target-s;
                attacks=diff==side*15 || diff==side*17;
            } else if(pt==Knight) {
                const int diff=std::abs(target-s);
                attacks=diff==14 || diff==18 || diff==31 || diff==33;
            } else attacks=ray_attacks(cells,s,target,pt);
            if(attacks) { best=s; break; }
        }
        if(best>=0) return {best,side*pt};
    }
    return {};
}
}
int see(Board& b,Move m) {
    AXIOM_HOT(See,See);
    if(!m || !valid(m.from) || !valid(m.to) || !b.squares[m.from]) return 0;

    std::array<int,128> cells=b.squares;
    const int us=b.side;
    const int moving=std::abs(cells[m.from]);
    int gain[32]{};
    int depth=0;
    gain[0]=capture_gain(b,m);

    // Apply the candidate capture locally: no Board::push(), no history/hash work,
    // and no recursive legal move generation.
    cells[m.from]=0;
    if(moving==Pawn && m.to==b.ep && !cells[m.to]) cells[m.to-us*16]=0;
    int occupant=m.promotion?us*m.promotion:us*moving;
    cells[m.to]=occupant;

    int side=-us;
    while(depth<30) {
        const SeeAttacker a=least_attacker(cells,m.to,side);
        if(a.square<0) break;
        ++depth;
        gain[depth]=piece_value(std::abs(occupant))-gain[depth-1];

        // Standard SEE early exit: neither side benefits from extending this line.
        if(std::max(-gain[depth-1],gain[depth])<0) break;

        cells[a.square]=0;
        int next_piece=std::abs(a.piece);
        const int target_rank=m.to>>4;
        if(next_piece==Pawn && ((side==White && target_rank==7) || (side==Black && target_rank==0)))
            next_piece=Queen;
        occupant=side*next_piece;
        cells[m.to]=occupant;
        side=-side;
    }
    while(depth>0) {
        gain[depth-1]=-std::max(-gain[depth-1],gain[depth]);
        --depth;
    }
    return gain[0];
}
std::vector<std::string> tactical_labels(Board& b,Move m) {
    std::vector<std::string> labels;
    if(b.capture(m)) labels.push_back("capture"); if(m.promotion) labels.push_back("promotion");
    if(b.gives_check(m)) labels.push_back("check");
    if(b.capture(m) && see(b,m)<0) labels.push_back("losing_exchange_candidate");
    int us=b.side;
    auto attacks_from=[](const Board& pos,int from,int to) {
        if(!valid(from) || !valid(to) || from==to || !pos.squares[from]) return false;
        int pt=std::abs(pos.squares[from]), c=color(pos.squares[from]), diff=to-from;
        if(pt==Pawn) return diff==c*15 || diff==c*17;
        if(pt==Knight) return std::abs(diff)==14 || std::abs(diff)==18 || std::abs(diff)==31 || std::abs(diff)==33;
        int dx=(to&7)-(from&7),dy=(to>>4)-(from>>4);
        if(pt==King) return std::max(std::abs(dx),std::abs(dy))==1;
        bool diag=std::abs(dx)==std::abs(dy), straight=dx==0 || dy==0;
        if((pt==Bishop && !diag) || (pt==Rook && !straight) || (pt==Queen && !diag && !straight)) return false;
        int step=((dx>0)-(dx<0))+16*((dy>0)-(dy<0));
        for(int s=from+step;s!=to;s+=step) if(!valid(s) || pos.squares[s]) return false; return true;
    };
    int enemyKing=b.king_square(-us);
    bool previouslyCheckedByOther=false;
    for(int s=0;s<128;++s) if(valid(s) && color(b.squares[s])==us && s!=m.from && attacks_from(b,s,enemyKing)) previouslyCheckedByOther=true;
    auto u=b.push(m);
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]==us*Queen && b.attacked(s,-us) && !b.attacked(s,us)) labels.push_back("undefended_queen_"+square_name(s));
    int forkTargets=0;
    for(int s=0;s<128;++s) if(valid(s) && color(b.squares[s])==-us && std::abs(b.squares[s])!=Pawn && attacks_from(b,m.to,s)) ++forkTargets;
    if(forkTargets>=2) labels.push_back("fork_candidate");
    for(int s=0;s<128;++s) if(valid(s) && color(b.squares[s])==us) {
        int pt=std::abs(b.squares[s]);
        if(s!=m.to && !previouslyCheckedByOther && attacks_from(b,s,enemyKing)) labels.push_back("discovered_check");
        if(pt!=Bishop && pt!=Rook && pt!=Queen) continue;
        for(int dir:{1,-1,16,-16,15,-15,17,-17}) {
            bool diag=std::abs(dir)==15 || std::abs(dir)==17;
            if((pt==Bishop && !diag) || (pt==Rook && diag)) continue;
            int first=-1;
            for(int t=s+dir;valid(t);t+=dir) if(b.squares[t]) {
                if(color(b.squares[t])==us) break;
                if(first<0) { first=t; continue; }
                if(std::abs(b.squares[t])==King) labels.push_back("absolute_pin_"+square_name(first));
                else if(piece_value(b.squares[first])>piece_value(b.squares[t])) labels.push_back("skewer_candidate");
                break;
            }
        }
    }
    bool advancedWhitePawn=false,advancedBlackPawn=false;
    for(int s=0;s<128;++s) if(valid(s)) {
        if(b.squares[s]==Pawn && (s>>4)>=5) advancedWhitePawn=true;
        if(b.squares[s]==-Pawn && (s>>4)<=2) advancedBlackPawn=true;
    }
    if(advancedWhitePawn && advancedBlackPawn) labels.push_back("promotion_race_candidate");
    auto replies=b.legal_moves();
    for(int s=0;s<128;++s) if(valid(s) && color(b.squares[s])==-us && std::abs(b.squares[s])!=Pawn && std::abs(b.squares[s])!=King && b.attacked(s,us)) {
        bool safe=false;
        for(auto reply:replies) if(reply.from==s) { auto undo=b.push(reply); safe=!b.attacked(reply.to,us); b.pop(undo); if(safe) break; }
        if(!safe) labels.push_back("trapped_piece_candidate_"+square_name(s));
    }
    if(b.in_check()) {
        labels.push_back("mating_attack_candidate");
        if(!u.squares[m.to] && !m.promotion) labels.push_back("checking_intermezzo_candidate");
    }
    std::sort(labels.begin(),labels.end()); labels.erase(std::unique(labels.begin(),labels.end()),labels.end());
    b.pop(u); return labels;
}
std::string status_name(Status s) { switch(s) {
    case Status::ExactTablebase:return "EXACT_TABLEBASE"; case Status::ProvenMate:return "PROVEN_MATE";
    case Status::ProvenForcedResult:return "PROVEN_FORCED_RESULT"; case Status::SearchResult:return "SEARCH_RESULT";
    default:return "HEURISTIC_EVALUATION"; } }
std::string bound_name(Bound b) { switch(b) { case Bound::Exact:return "EXACT_SEARCH"; case Bound::Lower:return "LOWER"; case Bound::Upper:return "UPPER"; default:return "ESTIMATE"; } }
}
