#pragma once
#include "position.hpp"
namespace ax2 {
constexpr int Tokens=768;
inline int token(int pc,int to){return (side_of(pc)*6+type(pc)-1)*64+to;}
struct Histories {
    std::int16_t main[2][64][64]{};
    std::int16_t captures[2][7][64][7]{};
    std::array<std::int16_t,Tokens*Tokens> continuation{};
    std::array<Move,Tokens> counters{};
    Move killers[MaxPly][2]{};
    static void update(std::int16_t& h,int bonus){bonus=std::clamp(bonus,-2000,2000);h=std::int16_t(h+bonus-int(h)*std::abs(bonus)/16000);}
    int quiet(const Position& p,Move m,int prev) const {
        int t=token(p.board[m.from()],m.to());return main[p.side][m.from()][m.to()]+(prev>=0?continuation[prev*Tokens+t]:0);
    }
    void reward(const Position& p,Move m,int prev,int bonus){
        if(p.capture(m))update(captures[p.side][type(p.board[m.from()])][m.to()][p.victim(m)],bonus);
        else {update(main[p.side][m.from()][m.to()],bonus);if(prev>=0)update(continuation[prev*Tokens+token(p.board[m.from()],m.to())],bonus);}
    }
};
struct Refutation {U64 key=0;Move move{};};
struct RefutationCache {
    std::array<Refutation,4096> entries{};
    Move probe(U64 key) const{const auto& e=entries[key&4095];return e.key==key?e.move:Move{};}
    void store(U64 key,Move m){entries[key&4095]={key,m};}
};
}
