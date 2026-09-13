#include "axiom/engine.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace axiom {
namespace {
std::uint64_t bit(int square) { return 1ULL<<((square>>4)*8+(square&7)); }
enum Term { Material,Mobility,KingSafety,PawnStructure,PieceActivity,Space,Threats,PassedPawns,EndgameTerms,
    Outposts,WeakSquares,BadBishop,RookFiles,PawnBreaks,Coordination,TrappedPieces,PassedPotential,KingRingPressure,KingOpenFiles,KingCoordination,Count };
constexpr const char* names[]={"Material","Mobility","KingSafety","PawnStructure","PieceActivity","Space","Threats","PassedPawns","EndgameTerms",
    "Outposts","WeakSquares","BadBishop","RookFiles","PawnBreaks","Coordination","TrappedPieces","PassedPotential","KingRingPressure","KingOpenFiles","KingCoordination"};
struct AttackMap { std::uint64_t from[128]{},occupied[2]{}; unsigned char count[2][128]{}; };
AttackMap attacks(const Board& b) {
    AXIOM_HOT(AttackMap,Inherit);
    AttackMap map;
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]) map.occupied[b.squares[s]>0?0:1]|=bit(s);
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]) {
        int piece=std::abs(b.squares[s]),colorIndex=b.squares[s]>0?0:1,side=color(b.squares[s]);
        auto add=[&](int t) { if(valid(t)) { map.from[s]|=bit(t); ++map.count[colorIndex][t]; } };
        if(piece==Pawn) { add(s+side*15); add(s+side*17); continue; }
        constexpr int knights[]={33,31,18,14,-14,-18,-31,-33};
        constexpr int sliders[]={17,16,15,1,-1,-15,-16,-17};
        const int* dirs=piece==Knight?knights:sliders;
        for(int i=0;i<8;++i) {
            int d=dirs[i]; bool diagonal=std::abs(d)==15 || std::abs(d)==17;
            if((piece==Bishop && !diagonal) || (piece==Rook && diagonal)) continue;
            for(int t=s+d;valid(t);t+=d) { add(t); if(b.squares[t] || piece==Knight || piece==King) break; }
        }
    } return map;
}
std::array<std::uint64_t,2> pawn_boards(const Board& b) {
    std::array<std::uint64_t,2> boards{};
    for(int s=0;s<128;++s) if(valid(s) && std::abs(b.squares[s])==Pawn) boards[b.squares[s]>0?0:1]|=bit(s);
    return boards;
}
}
PawnInfo PawnCache::compute(const Board& b) {
    AXIOM_HOT(PawnCompute,Inherit);
    PawnInfo info; info.pawns=pawn_boards(b);
    for(int side:{White,Black}) {
        int c=side==White?0:1;
        for(int s=0;s<128;++s) if(valid(s) && b.squares[s]==side*Pawn) {
            ++info.files[c][s&7]; ++info.rank_count[c][side==White?s>>4:7-(s>>4)];
            ++info.same_color[c][((s&7)+(s>>4))&1];
            for(int d:{15*side,17*side}) if(valid(s+d)) info.attacks[c]|=bit(s+d);
            for(int t=s;valid(t);t+=16*side) for(int d:{15*side,17*side}) if(valid(t+d)) info.future_attacks[c]|=bit(t+d);
        }
    }
    for(int side:{White,Black}) {
        int c=side==White?0:1;
        for(int s=0;s<128;++s) if(valid(s) && b.squares[s]==side*Pawn) {
            int file=s&7,rank=side==White?s>>4:7-(s>>4);
            bool isolated=(file==0 || info.files[c][file-1]==0) && (file==7 || info.files[c][file+1]==0);
            info.structure[c]-=12*isolated+8*(info.files[c][file]>1);
            int stoppers=0; bool sameFile=false;
            for(int f=std::max(0,file-1);f<=std::min(7,file+1);++f)
                for(int r=(s>>4)+side;r>=0 && r<8;r+=side) if(b.squares[r*16+f]==-side*Pawn) { ++stoppers; if(f==file) sameFile=true; }
            if(!stoppers) { info.passed[c]|=bit(s); ++info.passed_rank[c][rank]; }
            else if(!sameFile && (info.attacks[c]&bit(s)) && stoppers<=2) info.potential[c]+=rank*rank;
        }
    } return info;
}
const PawnInfo& PawnCache::probe(const Board& b) {
    AXIOM_HOT(PawnProbe,Inherit);
    auto boards=pawn_boards(b); std::uint64_t h=boards[0]^(std::rotl(boards[1],23)*0x9e3779b97f4a7c15ULL);
    auto& entry=entries_[(h^(h>>32))&(entries_.size()-1)];
    if(entry.valid && entry.info.pawns==boards) { ++hits; return entry.info; }
    ++misses; entry.info=compute(b); entry.valid=true; return entry.info;
}
void PawnCache::clear() { for(auto& entry:entries_) entry.valid=false; hits=misses=0; }
int evaluate_score(const Board& b,bool strategic,bool enhanced_king,PawnCache* cache,Evaluation* debug) {
    AXIOM_HOT(Evaluation,Evaluation);
    std::array<int,Count> terms{};
    PawnInfo scratch; const PawnInfo* pawns;
    if(cache) pawns=&cache->probe(b); else { scratch=PawnCache::compute(b); pawns=&scratch; }
    AttackMap map=attacks(b);
    int phase=0,bishops[2]{}; constexpr int phaseWeights[]={0,0,1,1,2,4,0};
    for(int s=0;s<128;++s) if(valid(s)) { phase+=phaseWeights[std::abs(b.squares[s])]; if(std::abs(b.squares[s])==Bishop) ++bishops[b.squares[s]>0?0:1]; }
    phase=std::min(24,phase);
    for(int c=0;c<2;++c) {
        int sign=c==0?1:-1;
        terms[PawnStructure]+=sign*pawns->structure[c];
        for(int r=0;r<8;++r) {
            terms[PassedPawns]+=sign*pawns->passed_rank[c][r]*(r*r*(120-3*phase)/24);
            terms[Space]+=sign*pawns->rank_count[c][r]*(std::max(0,r-2)*phase/8);
        }
        if(bishops[c]>=2) terms[PieceActivity]+=sign*25;
        if(strategic) terms[PassedPotential]+=sign*pawns->potential[c]*(48-phase)/24;
    }
    for(int s=0;s<128;++s) if(valid(s) && b.squares[s]) {
        int pt=std::abs(b.squares[s]),c=b.squares[s]>0?0:1,other=1-c,sign=c==0?1:-1;
        int rank=c==0?s>>4:7-(s>>4),file=s&7;
        int central=14-std::abs(2*file-7)-std::abs(2*(s>>4)-7);
        auto destinations=map.from[s]&~map.occupied[c];
        if(pt!=King) {
            terms[Material]+=sign*piece_value(pt);
            terms[PieceActivity]+=sign*central*(pt==Knight || pt==Bishop?3:1);
            if(map.count[other][s]) terms[Threats]-=sign*piece_value(pt)/(map.count[c][s]?40:8);
        }
        if(pt!=Pawn && pt!=King) terms[Mobility]+=sign*std::popcount(destinations)*3;
        if(pt==King) {
            AXIOM_HOT(KingEval,Inherit);
            std::uint64_t ring=map.from[s]|bit(s); int danger=0;
            for(int d:{0,1,-1,16,-16,15,-15,17,-17}) if(valid(s+d) && map.count[other][s+d]) ++danger;
            terms[EndgameTerms]+=sign*central*4*(24-phase)/24;
            if(!enhanced_king) terms[KingSafety]-=sign*danger*12*phase/24;
            else {
                int units=0,attackers=0; bool queen=false,rook=false; constexpr int weights[]={0,1,2,2,3,5,0};
                for(int a=0;a<128;++a) if(valid(a) && color(b.squares[a])==-sign && (map.from[a]&ring)) {
                    int type=std::abs(b.squares[a]); if(type==King) continue;
                    units+=weights[type]*std::min(3,std::popcount(map.from[a]&ring)); ++attackers;
                    queen|=type==Queen; rook|=type==Rook;
                }
                int pressure=(units*units/4+danger*6)*std::min(3,attackers)/3;
                if(!queen) pressure/=2;
                terms[KingRingPressure]-=sign*pressure*phase/24;
                for(int f=std::max(0,file-1);f<=std::min(7,file+1);++f) {
                    int open=pawns->files[c][f]==0?10:0;
                    if(pawns->files[other][f]==0) open+=4;
                    terms[KingOpenFiles]-=sign*open*phase/24;
                    int front=(s>>4)+sign;
                    if(front>=0 && front<8 && b.squares[front*16+f]!=sign*Pawn) terms[KingSafety]-=sign*5*phase/24;
                }
                if(queen && rook) terms[KingCoordination]-=sign*30*phase/24;
            }
        }
        if(!strategic) continue;
        if((pt==Knight || pt==Bishop) && rank>=3 && rank<=5 && (pawns->attacks[c]&bit(s)) && !(pawns->future_attacks[other]&bit(s))) terms[Outposts]+=sign*(pt==Knight?28:16);
        if(pt==Bishop) terms[BadBishop]-=sign*pawns->same_color[c][((s&7)+(s>>4))&1]*3;
        if(pt==Rook && !pawns->files[c][file]) terms[RookFiles]+=sign*(pawns->files[other][file]?12:22);
        if(pt==Pawn && (map.from[s]&pawns->pawns[other])) terms[PawnBreaks]+=sign*8*phase/24;
        if(pt!=Pawn && pt!=King && map.count[c][s]) terms[Coordination]+=sign*6;
        if(pt!=Pawn && pt!=King && map.count[other][s]) {
            int safe=0;
            for(int t=0;t<128;++t) if(valid(t) && (destinations&bit(t)) && !map.count[other][t]) ++safe;
            if(!safe) terms[TrappedPieces]-=sign*piece_value(pt)/10;
        }
    }
    if(strategic) for(int c=0;c<2;++c) {
        int sign=c==0?1:-1;
        for(int r=2;r<=5;++r) for(int f=2;f<=5;++f) {
            int s=r*16+f;
            if(map.count[c][s] && !(pawns->attacks[1-c]&bit(s))) terms[Space]+=sign*2*phase/24;
            if(map.count[1-c][s]>=2 && !(pawns->future_attacks[c]&bit(s))) terms[WeakSquares]-=sign*4*phase/24;
        }
    }
    int total=0;
    if(debug) { debug->terms.clear(); debug->phase=phase/24.0; }
    for(int t=0;t<Count;++t) { int value=terms[t]*b.side; total+=value; if(debug) debug->terms[names[t]]=value; }
    total=std::clamp(total,-MateThreshold+1,MateThreshold-1);
    if(debug) debug->total=total; return total;
}
bool strategic_candidate(const Board& b,Move move,const PawnInfo& pawns) {
    int pt=std::abs(b.squares[move.from]),c=b.side==White?0:1;
    int rank=b.side==White?move.to>>4:7-(move.to>>4);
    if(pt==King) return std::abs(move.to-move.from)==2 || b.piece_count()<=8;
    if(pt==Pawn) {
        if((pawns.passed[c]&bit(move.from)) && rank>=3) return true;
        for(int d:{15*b.side,17*b.side}) if(valid(move.to+d) && b.squares[move.to+d]==-b.side*Pawn) return true;
    }
    if((pt==Knight || pt==Bishop) && rank>=3 && (pawns.attacks[c]&bit(move.to)) && !(pawns.future_attacks[1-c]&bit(move.to))) return true;
    return pt==Rook && !pawns.files[c][move.to&7];
}
}
