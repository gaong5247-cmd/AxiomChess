#pragma once
#include <algorithm>
#include <cmath>

namespace ax2 {

// Shared search-policy input.  This deliberately contains search signals rather
// than engine-specific heuristic names so pruning/reduction/extension decisions
// can consume the same evidence without coupling the heuristics together.
struct PolicyInput {
    int depth=0;
    int moveIndex=0;
    int moveCount=0;
    int history=0;
    int eval=0;
    int beta=0;
    bool pv=false;
    bool ttPv=false;
    bool improving=false;
    bool unstable=false;
    bool cut=false;
    bool counter=false;
    bool refutation=false;
    bool endgame=false;
    bool inCheck=false;
    bool givesCheck=false;
};

struct SearchPolicy {
    // Positive confidence means "spend nodes here". Negative confidence means
    // the branch is a better candidate for reduction/pruning.
    static int branch_confidence(const PolicyInput& x) {
        int c=0;
        c += x.pv ? 28 : 0;
        c += x.ttPv ? 14 : 0;
        c += x.improving ? 8 : -3;
        c += x.unstable ? 18 : 0;
        c += x.counter ? 12 : 0;
        c += x.refutation ? 10 : 0;
        c += x.givesCheck ? 18 : 0;
        c += x.endgame ? 8 : 0;
        c += std::clamp(x.history / 700, -22, 22);
        c -= x.cut ? 7 : 0;
        c -= std::clamp(x.moveIndex - 4, 0, 14);
        if (x.moveCount <= 4) c += 7;
        return std::clamp(c, -72, 72);
    }

    static int lmr_reduction(const PolicyInput& x, int nextDepth) {
        if (x.depth < 3 || x.moveIndex < 3 || nextDepth <= 1) return 0;
        const double base = 0.55
            + std::log(double(std::max(2, x.depth)))
            * std::log(double(std::max(2, x.moveIndex + 1))) / 2.30;
        const int confidence = branch_confidence(x);
        double r = base;
        r += x.cut ? 0.55 : 0.0;
        r -= x.pv ? 0.45 : 0.0;
        r -= x.improving ? 0.35 : 0.0;
        r -= double(confidence) / 26.0;
        if (confidence < -38) r += 0.65;
        return std::clamp(int(std::floor(r)), 0, std::max(0, nextDepth - 1));
    }

    static int null_reduction(const PolicyInput& x) {
        int r = 2 + x.depth / 4;
        const int surplus = x.eval - x.beta;
        if (surplus > 160) ++r;
        if (surplus > 380) ++r;
        if (x.improving) ++r;
        if (x.unstable) --r;
        if (x.endgame) --r;
        return std::clamp(r, 2, std::max(2, x.depth - 1));
    }

    static int rfp_margin(const PolicyInput& x) {
        int margin = 92 * x.depth;
        margin += x.unstable ? 55 : 0;
        margin += x.endgame ? 45 : 0;
        margin -= x.improving ? 18 : 0;
        return std::max(40, margin);
    }

    static int probcut_margin(const PolicyInput& x) {
        int margin = 185;
        margin -= x.improving ? 25 : 0;
        margin += x.unstable ? 40 : 0;
        margin += std::max(0, x.depth - 6) * 5;
        return std::clamp(margin, 120, 320);
    }

    static int singular_margin(const PolicyInput& x) {
        int margin = 2 * x.depth;
        margin += x.ttPv ? x.depth / 2 : 0;
        margin += x.unstable ? x.depth / 3 : 0;
        return std::max(2, margin);
    }
};

} // namespace ax2
