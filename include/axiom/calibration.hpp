#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>

// All event counters are worker-local; timers are opt-in and inclusive.
#define AXIOM_CALIBRATION_FIELDS(X) \
 X(tt_probes) X(iid_nodes) X(iid_moves_found) X(singular_excluded_nodes) \
 X(singular_branch_nodes) X(singular_beta_cutoffs) X(singular_pv_updates) \
 X(null_fail_highs) X(null_verification_success) X(null_verification_nodes) X(null_material_rejections) \
 X(probcut_qsuccess) X(probcut_reduced_success) X(probcut_nodes) \
 X(rfp_attempts) X(history_reductions) X(lmr_reduction_sum) X(lmr_unverified_fail_lows) \
 X(aspiration_fail_low) X(aspiration_fail_high) X(soft_time_stops) \
 X(order_ns) X(eval_ns) X(correction_ns) X(tt_ns) X(legal_ns) X(qsearch_ns) \
 X(q_generated) X(q_searched) X(q_captures) X(q_promotions) X(q_checks) \
 X(q_delta_prunes) X(q_see_prunes) X(q_beta_cutoffs) X(q_stand_cutoffs) X(q_checked_nodes)

namespace axiom {
struct CalibrationStats {
#define AXIOM_DECLARE_COUNTER(name) std::uint64_t name=0;
    AXIOM_CALIBRATION_FIELDS(AXIOM_DECLARE_COUNTER)
#undef AXIOM_DECLARE_COUNTER
};
struct ProfileScope {
    std::uint64_t* output;
    std::chrono::steady_clock::time_point start;
    ProfileScope(bool enabled,std::uint64_t& target):output(enabled?&target:nullptr) {
        if(output) start=std::chrono::steady_clock::now();
    }
    ~ProfileScope() { if(output) *output+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count(); }
};
inline int eval_trend(int current,int previous,int unavailable=32000) {
    return current==unavailable || previous==unavailable?0:std::clamp(current-previous,-256,256);
}
inline int clock_available(int remaining,bool adaptive) {
    // Reserve transport/scheduling headroom; very short clocks cannot safely
    // spend the last few milliseconds searching even when increments exist.
    const int reserve=adaptive?std::max(75,remaining/20):25;
    return std::max(1,remaining-reserve);
}
inline double next_iteration_cost(double current,double previous) {
    return std::max(1.0,current)*std::clamp(previous>0?current/previous:3.0,1.5,8.0);
}
}
