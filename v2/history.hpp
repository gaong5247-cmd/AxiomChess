#pragma once
#include "position.hpp"
namespace ax2 {
constexpr int Tokens=768;
constexpr std::size_t FollowupSize=1u<<18;
constexpr std::size_t PawnHistorySize=1u<<18;
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

    int quiet(const Position& p,Move m,int prev1,int prev2=-1) const {
        int t=token(p.board[m.from()],m.to());
        int score=main[p.side][m.from()][m.to()];
        if(prev1>=0) score+=continuation[prev1*Tokens+t];
        if(prev2>=0) score+=followup[followup_index(prev2,t)]/2;
        score+=pawn[pawn_index(p,t)]/2;
        return score;
    }

    void reward(const Position& p,Move m,int prev1,int bonus,int prev2=-1){
        if(p.capture(m)) {
            update(captures[p.side][type(p.board[m.from()])][m.to()][p.victim(m)],bonus);
            return;
        }
        int t=token(p.board[m.from()],m.to());
        update(main[p.side][m.from()][m.to()],bonus);
        if(prev1>=0) update(continuation[prev1*Tokens+t],bonus);
        if(prev2>=0) update(followup[followup_index(prev2,t)],bonus/2);
        update(pawn[pawn_index(p,t)],bonus/2);
    }
};
struct Refutation {U64 key=0;Move move{};};
struct RefutationCache {
    std::array<Refutation,4096> entries{};
    Move probe(U64 key) const{const auto& e=entries[key&4095];return e.key==key?e.move:Move{};}
    void store(U64 key,Move m){entries[key&4095]={key,m};}
};
}
