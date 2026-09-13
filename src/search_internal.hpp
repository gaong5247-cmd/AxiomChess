#pragma once
#include "axiom/chess.hpp"
namespace axiom::search_detail {
struct Interrupted {};
struct Applied { Board& b; Undo undo; Applied(Board& board,Move m):b(board),undo(m?b.push(m):b.push_null()) {} ~Applied(){b.pop(undo);} };
}
