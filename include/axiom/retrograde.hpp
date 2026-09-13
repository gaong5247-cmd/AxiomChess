#pragma once
#include <vector>
namespace axiom {
// Finite, complete, alternating-turn graph. Terminal labels use player-to-move
// perspective: -1 loss, 0 draw, +1 win; 2 means not a terminal.
// A chess enumerator must include castling, EP and draw-rule state in each ID.
struct RetroState { int result=0; int distance=-1; };
std::vector<RetroState> retrograde(const std::vector<std::vector<int>>& successors,
                                  const std::vector<int>& terminals);
}
