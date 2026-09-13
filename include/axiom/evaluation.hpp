#pragma once
#include "axiom/chess.hpp"
#include <array>
#include <vector>
namespace axiom {
struct Evaluation;
struct PawnInfo {
    std::array<std::uint64_t,2> pawns{},attacks{},future_attacks{},passed{};
    int files[2][8]{},rank_count[2][8]{},passed_rank[2][8]{};
    int structure[2]{},same_color[2][2]{},potential[2]{};
};
class PawnCache {
public:
    PawnCache():entries_(4096) {}
    const PawnInfo& probe(const Board& b);
    void clear();
    std::uint64_t hits=0,misses=0;
    static PawnInfo compute(const Board& b);
private:
    struct Entry { bool valid=false; PawnInfo info; };
    std::vector<Entry> entries_;
};
int evaluate_score(const Board& b,bool strategic=false,bool king_safety=false,PawnCache* cache=nullptr,Evaluation* debug=nullptr);
bool strategic_candidate(const Board& b,Move move,const PawnInfo& pawns);
struct SafetySignals {
    bool pv=false,tt=false,check=false,promotion=false,high_history=false;
    bool singular=false,strategic=false,improving=false,threat=false;
    bool protect() const { return pv || tt || check || promotion || high_history || singular || strategic || improving || threat; }
};
}
