#pragma once
#include "types.hpp"
namespace ax2 {
struct AttackTables {
    std::array<U64,64> knight{}, king{};
    std::array<std::array<U64,64>,2> pawn{};
    std::array<std::array<U64,64>,64> between{}, line{};
    AttackTables();
};
extern const AttackTables attacks;
U64 bishop_attacks(int s,U64 occ);
U64 rook_attacks(int s,U64 occ);
inline U64 piece_attacks(int pt,int s,U64 occ) {
    switch(pt) {
    case Knight:return attacks.knight[s];
    case Bishop:return bishop_attacks(s,occ);
    case Rook:return rook_attacks(s,occ);
    case Queen:return bishop_attacks(s,occ)|rook_attacks(s,occ);
    case King:return attacks.king[s];
    default:return 0;
    }
}
}
