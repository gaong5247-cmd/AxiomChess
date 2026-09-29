#pragma once
#include "axiom/chess.hpp"
#include "axiom/evaluation.hpp"
#include "axiom/calibration.hpp"
#include "axiom/safety.hpp"
#include "axiom/causal_trace.hpp"
#include "axiom/hot_profile.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace axiom {
constexpr int MateScore=30000, MateThreshold=29000, Infinity=32000, MaxPly=120;
constexpr int score_to_tt(int s,int ply) { return s>MateThreshold?s+ply:s<-MateThreshold?s-ply:s; }
constexpr int score_from_tt(int s,int ply) { return s>MateThreshold?s-ply:s<-MateThreshold?s+ply:s; }
namespace SearchTuning {
constexpr int RfpMaxDepth=3, RfpBaseMargin=90, RfpDepthMargin=100;
constexpr int UnstableEvalDelta=120, ProbCutBaseMargin=140, ProbCutImprovingMargin=40;
}
int piece_value(int piece);
struct Evaluation { int total=0; double phase=0; std::map<std::string,int> terms; };
Evaluation evaluate(const Board& b);
int see(Board& b,Move m);
std::vector<std::string> tactical_labels(Board& b,Move m);
enum class Status { ExactTablebase, ProvenMate, ProvenForcedResult, SearchResult, HeuristicEvaluation };
std::string status_name(Status s);
enum class Bound { Exact, Lower, Upper, Estimate };
std::string bound_name(Bound b);
struct Features {
    bool correction=false, continuation=false, capture_history=false, countermove=false;
    bool singular=false, verified_null=false, probcut=false, dynamic_lmr=false;
    bool history_pruning=false, see_pruning=false, mate_distance=false, iid=false;
    bool tt_policy=false, time_management=false;
    bool strategic_eval=false, king_safety=false, search_safety=false, pawn_cache=true;
    bool rfp=false;
    bool trend_safety=false, calibrated_lmr=false, adaptive_time=false;
    bool reuse_move_facts=false;
    bool legal_fast_path=false;
    bool set(const std::string& name,bool enabled);
    void all(bool enabled);
    void middlegame();
    std::string names() const;
};
struct SearchStats : CalibrationStats {
    SafetyStats safety;
    std::array<std::uint64_t,MaxPly> qply_histogram{};
    std::uint64_t correction_updates=0, continuation_updates=0, capture_updates=0;
    std::uint64_t tt_hits=0, null_attempts=0, null_verifications=0, null_rejections=0;
    std::uint64_t singular_tests=0, singular_extensions=0, probcut_attempts=0, probcut_cutoffs=0;
    std::uint64_t lmr_reductions=0, lmr_researches=0, history_prunes=0, see_prunes=0;
    std::uint64_t mate_distance_cutoffs=0, iid_searches=0, bestmove_changes=0;
    int root_correction=0, threads=1,seldepth=0;
    std::uint64_t safety_protected=0,pawn_cache_hits=0,pawn_cache_misses=0;
    std::uint64_t qnodes=0,tt_cutoffs=0,null_cutoffs=0,pvs_scouts=0,pvs_researches=0;
    std::uint64_t lmr_verifications_completed=0,horizon_interruptions=0,qcheck_evasions=0;
    std::uint64_t rfp_cuts=0;
};
struct TTEntry {
    std::uint64_t key=0;
    std::uint64_t context=0;
    int depth=-1,score=0;
    Bound bound=Bound::Estimate;
    Move move;
    bool pv=false;
    unsigned generation=0;
};
class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t mb=32);
    std::optional<TTEntry> probe(std::uint64_t key,std::uint64_t context) const;
    void store(TTEntry entry,bool policy);
    void clear();
    void new_search() { ++generation_; }
    int hashfull() const;
    unsigned generation() const { return generation_; }
    std::size_t buckets() const { return buckets_.size(); }
private:
    struct Bucket { std::array<TTEntry,4> entries; };
    std::vector<Bucket> buckets_;
    mutable std::array<std::mutex,64> locks_;
    unsigned generation_=0;
};
struct SearchTrace { Move move; int piece=0,static_eval=Infinity; };
class Feedback {
public:
    Feedback();
    void clear();
    int correction(const Board& b,int previous,unsigned mask=31) const;
    std::array<int,5> correction_components(const Board& b,int previous) const;
    const std::array<std::vector<std::int16_t>,5>& correction_state() const { return corrections_; }
    void update_correction(const Board& b,int previous,int error,int depth);
    int continuation(int previous,int current,int distance) const;
    void update_continuation(int previous,int current,int distance,int bonus);
    int capture_score(int piece,int to,int victim) const;
    void update_capture(int piece,int to,int victim,int bonus);
    Move counter(int previous) const;
    void set_counter(int previous,Move move);
    static int token(int piece,int to);
    static void update(std::int16_t& value,int bonus,int limit=16000);
private:
    std::array<std::vector<std::int16_t>,5> corrections_;
    std::array<std::vector<std::int16_t>,4> continuations_;
    std::vector<std::int16_t> captures_;
    std::array<Move,769> counters_{};
    std::array<std::size_t,5> correction_keys(const Board& b,int previous) const;
};
struct ProofNode {
    // Attacker: one selected edge. Defender: every legal edge.
    std::vector<std::pair<Move,std::shared_ptr<ProofNode>>> edges;
    int distance=0;
};
struct MateProof {
    bool proven=false, exhausted=false;
    int attacker=White, distance=-1;
    std::uint64_t nodes=0;
    std::shared_ptr<ProofNode> certificate;
};
class MateSolver {
public:
    MateProof solve(Board board,int attacker,int max_plies,std::uint64_t budget,
                    std::atomic_bool* stop=nullptr,
                    std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::time_point::max());
    static bool verify(Board board,int attacker,const std::shared_ptr<ProofNode>& certificate);
private:
    std::uint64_t nodes_=0, budget_=0;
    bool exhausted_=false;
    int attacker_=White;
    std::atomic_bool* stop_=nullptr;
    std::chrono::steady_clock::time_point deadline_;
    std::shared_ptr<ProofNode> visit(Board& b,int remaining);
};
struct TBResult { int wdl=0, dtz=0; bool exact=false; Move best; std::string reason; };
class Tablebase {
public:
    bool open(const std::string& path);
    std::optional<TBResult> probe(Board& b) const;
    bool enabled() const { return ready_; }
    static bool compiled();
private:
    bool ready_=false;
};
struct Limits {
    int depth=6, milliseconds=1000, mate_plies=5;
    std::uint64_t nodes=0, proof_nodes=15000;
    bool verify=false, selective=true;
    bool pvs=true, use_tt=true;
    Features features;
    int threads=1, soft_milliseconds=0;
    bool diagnostics=false, profile=false;
    unsigned correction_mask=31;
    unsigned safety_mask=SafetyLegacy;
    bool use_lmr=true, use_null=true;
    bool partial_ordering=false;
    bool time_guard=false;
    long long queue_ms=0;
    std::string force_root_move;
    int calibration_samples=0;
#ifdef AXIOM_RESEARCH_TRACE
    bool trace_search=false;
    std::string trace_root_move,trace_output;
    std::size_t trace_max_events=100000;
    int trace_min_iteration=0;
    bool replay_subtree=false;
    bool cost_profile=false;
    int replay_alpha=-Infinity,replay_beta=Infinity;
#endif
};
struct MoveResult {
    Move move; int score=0, depth=0, mate_distance=-1;
    Status status=Status::HeuristicEvaluation;
    Bound bound=Bound::Estimate;
    std::uint64_t nodes=0;
    std::vector<Move> pv;
    std::vector<std::string> tactics;
    bool tactical_verified=false;
    int verification_depth=0, verification_score=0;
    Move strongest_reply;
    std::optional<TBResult> tablebase;
};
struct CorrectionSample { int raw=0,corrected=0,score=0,pieces=0; std::array<int,5> components{}; };
struct RootMoveInfo {
    Move move; int score=0,previous_score=0,depth=0,observations=0,stable_iterations=0;
    std::uint64_t nodes=0; double mean=0,m2=0; std::vector<Move> pv;
    void update(const MoveResult& r) {
        previous_score=score;
        stable_iterations=observations && std::abs(r.score-score)<=20 && pv==r.pv?stable_iterations+1:0;
        score=r.score; depth=r.depth; nodes+=r.nodes; pv=r.pv; ++observations;
        double delta=score-mean; mean+=delta/observations; m2+=delta*(score-mean);
    }
};
struct LmrSample {
    std::string fen; Move move,root_move;
    int depth=0,reduction=0,index=0,history=0,continuation=0,capture_history=0,eval=0,trend=0,reduced_score=0,score=0;
    bool researched=false;
};
struct SearchResult {
#ifdef AXIOM_RESEARCH_TRACE
    std::optional<HotProfile> hot_profile;
#endif
    Move best;
    Status status=Status::HeuristicEvaluation;
    std::string outcome="UNKNOWN";
    int depth=0, score=0,seldepth=0,hashfull=0;
    std::uint64_t nodes=0, proof_nodes=0;
    long long elapsed_ms=0;
    std::vector<MoveResult> moves;
    Evaluation evaluation;
    bool claim_draw=false, verification_completed=false, certificate_verified=false;
    std::string verification_detail;
    std::optional<TBResult> tablebase;
    SearchStats stats;
    std::vector<CorrectionSample> correction_samples;
    std::vector<LmrSample> lmr_samples;
    std::vector<RootMoveInfo> root_history;
    double time_factor=1.0,root_node_fraction=0;
    long long queue_ms=0,setup_ms=0,hard_overshoot_ms=0;
    double next_iteration_estimate_ms=0,iteration_estimate_error_ms=0;
    bool time_guard_stopped=false;
    std::string features;
    unsigned safety_mask=SafetyLegacy;
    std::string force_root_move;
};
class Search {
    friend struct SearchAudit;
public:
    explicit Search(std::size_t hash_mb=32);
    SearchResult run(Board b,Limits limits,std::atomic_bool& stop,
                     const std::function<void(const SearchResult&)>& info={});
    Tablebase tablebase;
    void clear();
    CorrectionSample frozen_correction(const Board& b,int previous=0) const;
    const std::array<std::vector<std::int16_t>,5>& correction_state() const { return feedback_->correction_state(); }
private:
    std::shared_ptr<TranspositionTable> tt_;
    std::unique_ptr<Feedback> feedback_;
    std::unique_ptr<PawnCache> pawn_cache_;
#ifdef AXIOM_RESEARCH_TRACE
    std::unique_ptr<CausalTrace> causal_trace_;
#endif
    std::array<SearchTrace,MaxPly> trace_{};
    mutable SearchStats stats_;
    std::vector<CorrectionSample> correction_samples_;
    std::vector<LmrSample> lmr_samples_;
    struct RankedMove { int score=0; std::size_t ordinal=0; Move move; };
    // Worker-owned scratch; ordering finishes before recursive search begins.
    std::array<RankedMove,256> ordering_scratch_{};
    struct MoveFacts { Move move; int exchange=0; bool see_known=false,checking=false; };
    // Candidate-only allocation. One frame per ply; auxiliary same-ply probes
    // finish before this node's order(). Last frame is nonrecursive sort scratch.
    std::unique_ptr<MoveFacts[]> move_facts_;
    std::string previous_policy_;
    std::atomic_bool* local_stop_=nullptr;
    std::atomic<std::uint64_t>* shared_nodes_=nullptr;
    int worker_id_=0;
    Move killers_[MaxPly][2]{};
    int history_[2][128][128]{};
    Limits limits_;
    std::atomic_bool* stop_=nullptr;
    std::chrono::steady_clock::time_point start_,deadline_;
    std::uint64_t nodes_=0;
    void tick();
    int negamax(Board& b,int depth,int alpha,int beta,int ply,bool pv,bool null_allowed,std::vector<Move>& line,bool synthetic=false,Move excluded={},bool auxiliary=false,int extensions=0,bool cut_node=false);
    int quiescence(Board& b,int alpha,int beta,int ply,int qply,bool synthetic=false,bool clean=false);
    void order(Board& b,std::vector<Move>& moves,Move tt,int ply);
    std::vector<MoveResult> root(Board& b,int depth,int alpha,int beta,bool independent=false);
    SearchResult run_single(Board b,Limits limits,std::atomic_bool& stop,const std::function<void(const SearchResult&)>& info);
    int previous_token(int ply,int distance=1) const;
    int history_score(const Board& b,Move m,int ply) const;
    int corrected_eval(const Board& b,int raw,int ply) const;
    int static_evaluation(const Board& b);
};
std::string json(const SearchResult& result,const Board& b);
}
