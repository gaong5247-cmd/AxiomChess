#include "policy.hpp"
#include <iostream>
#include <stdexcept>
using namespace ax2;

static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}

int main(){
    try {
        PolicyInput base; base.depth=8;base.moveIndex=8;base.moveCount=24;base.history=0;base.eval=120;base.beta=100;
        auto quiet=base;
        auto critical=base;critical.pv=true;critical.ttPv=true;critical.unstable=true;critical.history=6000;critical.givesCheck=true;
        auto late=base;late.cut=true;late.moveIndex=18;late.history=-7000;
        require(SearchPolicy::branch_confidence(critical)>SearchPolicy::branch_confidence(quiet),"critical confidence ordering");
        require(SearchPolicy::branch_confidence(late)<SearchPolicy::branch_confidence(quiet),"late confidence ordering");

        const int nextDepth=7;
        require(SearchPolicy::lmr_reduction(critical,nextDepth)<=SearchPolicy::lmr_reduction(quiet,nextDepth),"critical move must not reduce more");
        require(SearchPolicy::lmr_reduction(late,nextDepth)>=SearchPolicy::lmr_reduction(quiet,nextDepth),"late bad-history move should reduce at least as much");

        auto unstable=base;unstable.unstable=true;
        require(SearchPolicy::null_reduction(unstable)<=SearchPolicy::null_reduction(base),"unstable node null reduction safety");
        require(SearchPolicy::rfp_margin(unstable)>SearchPolicy::rfp_margin(base),"unstable node wider RFP margin");
        require(SearchPolicy::probcut_margin(unstable)>SearchPolicy::probcut_margin(base),"unstable node wider ProbCut margin");

        auto tt=base;tt.ttPv=true;
        require(SearchPolicy::singular_margin(tt)>=SearchPolicy::singular_margin(base),"TT-PV singular margin");
        std::cout<<"PASS unified search policy invariants\n";
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
