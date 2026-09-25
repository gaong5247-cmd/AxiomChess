#pragma once
#include "position.hpp"
#include <optional>
namespace ax2 {
struct TBRoot {Move move;int score=0,wdl=0,dtz=0;};
class Tablebase {
    bool ready=false;
public:
    bool open(const std::string& path);
    static bool compiled();
    std::optional<int> wdl(const Position& p) const;
    std::optional<TBRoot> root(const Position& p) const;
};
}
