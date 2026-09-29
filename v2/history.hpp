#pragma once
#include "position.hpp"
namespace ax2 {
constexpr int Tokens=768;
constexpr std::size_t FollowupSize=1u<<18;
constexpr std::size_t PawnHistorySize=1u<<18;
constexpr std::size_t CorrectionSize=1u<<16;
constexpr std::size_t ThreatHistorySize=1u<<17;
constexpr int LowPlyHistoryDepth=5;
inline int token(int pc,int to){return (side_of(pc)*6+type(pc)-1)*64+to;}
struct Histories {
    std::int16_t main[2][64][64]{};
    std::int16_t captures[2][7][64][7]{};

    // Dense 1-ply continuation: previous piece->square -> current piece->square.
    std::array<std::int16_t,Tokens*Tokens> continuation{};

    // Compressed deeper context. Hashing avoids multiplying per-thread memory by
    // another full Tokens^2 table for every continuation distance.
    std::array<std::int16_t,FollowupSize> followup{};
    std::array<std::int16_t,PawnHistorySize> pawn{};
    std::array<std::int16_t,ThreatHistorySize> threat{};
    std::array<std::int16_t,CorrectionSize> correction{};
    std::int16_t lowPly[LowPlyHistoryDepth][2][64][64]{};

    std::array<Move,Tokens> counters{};
    Move killers[MaxPly][2]{};

    static void update(std::int16_t& h,int bonus){
        bonus=std::clamp(bonus,-2000,2000);
        h=std::int16_t(h+bonus-int(h)*std::abs(bonus)/16000);
    }
    static std::size_t followup_index(int previous2,int current){
        U64 x=U64(previous2+1)*0x9E3779B97F4A7C15ULL ^ U64(current+17)*0xBF58476D1CE4E5B9ULL;
        return mix(x)&(FollowupSize-1);
    }
    static U64 pawn_key(const Position& p){
        return mix(p.bb[White][Pawn] ^ (p.bb[Black][Pawn]*0x9E3779B97F4A7C15ULL));
    }
    static std::size_t pawn_index(const Position& p,int current){
        return mix(pawn_key(p)^U64(current+1)*0x94D049BB133111EBULL)&(PawnHistorySize-1);
    }
    static std::size_t correction_index(const Position& p){
        return mix(pawn_key(p)^U64(p.side+1)*0xD6E8FEB86659FD93ULL)&(CorrectionSize-1);
    }
    static std::size_t threat_index(const Position& p,Move m,int current){
        const U64 occ=p.occupancy();
        const U64 fromThreat=p.attackers(m.from(),p.side^1,occ);
        const U64 toThreat=p.attackers(m.to(),p.side^1,occ);
        U64 x=mix(fromThreat)^std::rotl(mix(toThreat),23)^U64(current+1)*0xA24BAED4963EE407ULL;
        return mix(x)&(ThreatHistorySize-1);
    }
    static bool has_threat_context(const Position& p,Move m){
        const U64 occ=p.occupancy();
        return p.attackers(m.from(),p.side^1,occ)||p.attackers(m.to(),p.side^1,occ);
    }
    int corrected_eval(const Position& p,int raw) const {
        return std::clamp(raw+int(correction[correction_index(p)])/8,-MateBound+1,MateBound-1);
    }
    void reward_correction(const Position& p,int error,int depth){
        // Store scaled centipawn error with gravity; deeper searches carry more weight.
        int bonus=std::clamp(error*std::min(depth,8)/2,-2000,2000);
        update(correction[correction_index(p)],bonus);
    }

    int quiet(const Position& p,Move m,int prev1,int prev2=-1,int ply=MaxPly) const {
        int t=token(p.board[m.from()],m.to());
        int score=main[p.side][m.from()][m.to()];
        if(prev1>=0) score+=continuation[prev1*Tokens+t];
        if(prev2>=0) score+=followup[followup_index(prev2,t)]/2;
        score+=pawn[pawn_index(p,t)]/2;
        if(ply<LowPlyHistoryDepth) score+=lowPly[ply][p.side][m.from()][m.to()];
        if(has_threat_context(p,m)) score+=threat[threat_index(p,m,t)]/2;
        return score;
    }

    void reward(const Position& p,Move m,int prev1,int bonus,int prev2=-1,int ply=MaxPly){
        if(p.capture(m)) {
            update(captures[p.side][type(p.board[m.from()])][m.to()][p.victim(m)],bonus);
            return;
        }
        int t=token(p.board[m.from()],m.to());
        update(main[p.side][m.from()][m.to()],bonus);
        if(prev1>=0) update(continuation[prev1*Tokens+t],bonus);
        if(prev2>=0) update(followup[followup_index(prev2,t)],bonus/2);
        update(pawn[pawn_index(p,t)],bonus/2);
        if(ply<LowPlyHistoryDepth) update(lowPly[ply][p.side][m.from()][m.to()],bonus);
        if(has_threat_context(p,m)) update(threat[threat_index(p,m,t)],bonus/2);
    }
};
struct Refutation {U64 key=0;Move move{};};
struct RefutationCache {
    std::array<Refutation,4096> entries{};
    Move probe(U64 key) const{const auto& e=entries[key&4095];return e.key==key?e.move:Move{};}
    void store(U64 key,Move m){entries[key&4095]={key,m};}
};
}
