#pragma once
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace ax2 {
using U64 = std::uint64_t;
enum Piece { None, Pawn, Knight, Bishop, Rook, Queen, King };
constexpr int White=0, Black=1, MaxPly=128, Mate=30000, MateBound=29000, Inf=32000;
constexpr int values[7]={0,100,320,335,500,950,20000};
constexpr int phaseWeight[7]={0,0,1,1,2,4,0};
constexpr U64 bit(int s) { return U64(1)<<s; }
inline int pop(U64& b) { assert(b); int s=std::countr_zero(b); b&=b-1; return s; }
inline int count(U64 b) { return std::popcount(b); }
constexpr int type(int pc) { return pc&7; }
constexpr int side_of(int pc) { return pc>>3; }
constexpr int piece(int c,int t) { return (c<<3)|t; }
constexpr int file(int s) { return s&7; }
constexpr int rank(int s) { return s>>3; }
constexpr U64 FileA=0x0101010101010101ULL;
constexpr U64 file_mask(int f) { return FileA<<f; }
struct Move {
    std::uint16_t v=0;
    constexpr Move()=default;
    constexpr Move(int from,int to,int promo=0):v(std::uint16_t(from|(to<<6)|(promo<<12))) {}
    constexpr int from() const { return v&63; }
    constexpr int to() const { return (v>>6)&63; }
    constexpr int promo() const { return v>>12; }
    explicit constexpr operator bool() const { return v!=0; }
    bool operator==(const Move&) const=default;
    std::string uci() const;
};
struct MoveList {
    std::array<Move,256> data{}; int size=0;
    void add(Move m) { if(size>=256)throw std::length_error("move list capacity");data[size++]=m; }
    Move* begin(){return data.data();} Move* end(){return data.data()+size;}
    const Move* begin() const{return data.data();} const Move* end() const{return data.data()+size;}
    Move operator[](int i) const {return data[i];}
};
inline int to_tt(int s,int ply) { return s>MateBound?s+ply:s<-MateBound?s-ply:s; }
inline int from_tt(int s,int ply) { return s>MateBound?s-ply:s<-MateBound?s+ply:s; }
std::string square(int s);
int parse_square(const std::string& s);
U64 mix(U64 x);
}
