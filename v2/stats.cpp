#include "search.hpp"
#include <sstream>
#include <cmath>
namespace ax2 {
bool Features::set(const std::string& name,bool on){
#define FEATURE(n,field) if(name==n){field=on;return true;}
    FEATURE("LMR",lmr) FEATURE("NullMove",nullMove) FEATURE("Singular",singular) FEATURE("ReverseFutility",rfp)
    FEATURE("Futility",futility) FEATURE("SEEPruning",see) FEATURE("DeltaPruning",delta) FEATURE("Razoring",razor)
    FEATURE("MoveCountPruning",moveCount) FEATURE("HistoryPruning",history) FEATURE("ProbCut",probcut)
    FEATURE("DecisionImpact",decision) FEATURE("RefutationSearch",refutation)
#undef FEATURE
    return false;
}
std::string Stats::text() const {std::ostringstream o;
#define STAT(n) o<<#n<<'='<<n<<' ';
    STAT(qnodes) STAT(ttHits) STAT(ttCuts) STAT(pvs) STAT(pvsResearch) STAT(lmr) STAT(lmrResearch)
    STAT(nullTries) STAT(nullVerify) STAT(nullReject) STAT(nullCuts) STAT(singularTries) STAT(singularExtensions)
    STAT(rfp) STAT(futility) STAT(see) STAT(delta) STAT(razor) STAT(moveCount) STAT(history) STAT(probcutTries) STAT(probcut)
    STAT(aspirationLow) STAT(aspirationHigh) STAT(decisionProbes) STAT(decisionNodes) STAT(refutationProbes) STAT(refutationNodes) STAT(refutations) STAT(tbHits)
    STAT(nullNodes) STAT(singularNodes) STAT(probcutNodes) STAT(lmrResearchNodes)
    STAT(decisionInternalProbes) STAT(decisionInternalNodes)
#undef STAT
    return o.str();
}
double RootMove::priority(int best) const {
    // Conspiracy-style approximation, not a computed conspiracy number.
    double gap=std::max(0,best-score);double impact=1.0/(1.0+gap/35.0);
    double uncertaintyFactor=20.0+uncertainty+20.0*failures+10.0*pvChurn+risk;
    return impact*uncertaintyFactor*(1.0+1.0/(1+stability))/std::sqrt(double(std::max<U64>(1,work)));
}
}
