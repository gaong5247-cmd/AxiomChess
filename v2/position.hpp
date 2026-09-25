#pragma once
#include "attacks.hpp"
namespace ax2 {
constexpr const char* StartFen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
struct Undo {
    U64 key=0, context=0;
    int castle=0,ep=-1,halfmove=0,fullmove=1,historySize=0,nullBoundary=0;
    int captured=0,capturedSquare=0,moved=0;
    bool repetitionPair=false;
};
class Position {
public:
    std::array<std::uint8_t,64> board{};
    std::array<std::array<U64,7>,2> bb{};
    std::array<U64,2> occupied{};
    int side=White,castle=15,ep=-1,halfmove=0,fullmove=1;
    U64 key=0,context=0;
    int mg=0,eg=0,phase=0;
    std::array<U64,2048> history{};
    int historySize=0,nullBoundary=0;
    bool repetitionPair=false;
    Position(); explicit Position(const std::string& fen);
    void set_fen(const std::string& fen);
    std::string fen() const;
    U64 occupancy() const{return occupied[0]|occupied[1];}
    int king(int c) const {assert(bb[c][King]);return std::countr_zero(bb[c][King]);}
    U64 attackers(int s,int c,U64 occ,U64 removed=0) const;
    bool attacked(int s,int c) const{return attackers(s,c,occupancy())!=0;}
    bool in_check() const {return attacked(king(side),side^1);}
    bool capture(Move m) const {return board[m.to()] || (type(board[m.from()])==Pawn&&m.to()==ep);}
    int victim(Move m) const {return board[m.to()]?type(board[m.to()]):capture(m)?Pawn:None;}
    void make(Move m,Undo& u); void unmake(Move m,const Undo& u);
    void make_null(Undo& u); void unmake_null(const Undo& u);
    MoveList legal_moves() const;
    Move parse_move(const std::string& text) const;
    int repetitions() const;
    bool insufficient() const;
    bool automatic_draw() const {return halfmove>=150 || repetitions()>=5 || insufficient();}
    bool claim_now() const {return halfmove>=100 || repetitions()>=3;}
    bool can_claim(const MoveList& legal);
    bool nonpawn(int c) const {return (occupied[c]&~(bb[c][Pawn]|bb[c][King]))!=0;}
    U64 recompute_key() const;
    bool consistent() const;
    U64 tt_key() const {return key^mix(context)^mix(U64(halfmove)+0x10000)^mix(U64(nullBoundary!=0)+0x20000);}
private:
    void put(int s,int pc); void remove(int s);
    int canonical_ep() const;
    void save(Undo& u) const;
};
std::uint64_t perft(Position& p,int depth);
int pst(int pc,int s,bool endgame);
}
