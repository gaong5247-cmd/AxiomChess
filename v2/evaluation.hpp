#pragma once
#include "position.hpp"
namespace ax2 {
int evaluate(const Position& p);
bool see_ge(const Position& p,Move m,int threshold);
}
