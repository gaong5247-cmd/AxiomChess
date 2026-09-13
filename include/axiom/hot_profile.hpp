#pragma once
#ifdef AXIOM_RESEARCH_TRACE
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <ostream>
namespace axiom {
enum class HotKind {Push,Pop,NullPush,Legal,Pseudo,Attack,GivesCheck,See,Exchange,LegalCaptures,Evaluation,AttackMap,PawnProbe,PawnCompute,ProofKey,Correction,Order,OrderScoring,OrderSort,QSearch,MainSearch,Proof,KingEval,StrategicEval,Phase,PawnTerms,Count};
enum class HotDomain {Inherit,Search,Legal,See,GivesCheck,Evaluation,QSearch,Proof,Order,Count};
constexpr std::array<const char*,26> HotNames={"push","pop","null_push","legal","pseudo","attacked","gives_check","see","see_exchange","legal_captures","evaluation","attack_map","pawn_probe","pawn_compute","proof_key","correction","ordering","order_scoring","order_sort","qsearch","main_search","proof","king_eval","strategic_eval","phase","pawn_terms"};
constexpr std::array<const char*,9> HotDomains={"other","search","legal","see","gives_check","evaluation","qsearch","proof","ordering"};
struct HotCounter {std::uint64_t calls=0,inclusive_ns=0,exclusive_ns=0;};
struct LegalClassCounter {std::uint64_t generated=0,legal=0,illegal=0,pushes=0,attacked=0,elapsed_ns=0;};
constexpr std::array<const char*,15> LegalClassNames={"pawn","knight","bishop","rook","queen","king","capture","quiet","promotion","castling","en_passant","in_check","non_check","pinned","unpinned"};
struct HotProfile {
    std::array<LegalClassCounter,15> legal_classes{};
    std::array<HotCounter,26> counters{};
    std::array<std::array<HotCounter,9>,26> sites{};
    HotDomain domain=HotDomain::Search;
    std::uint64_t board_copy_bytes=0;
    void json(std::ostream& out) const {
        out<<"{\"board_snapshot_bytes_copied\":"<<board_copy_bytes<<",\"counters\":{";
        auto counter=[&](const HotCounter& c){out<<"{\"calls\":"<<c.calls<<",\"inclusive_ns\":"<<c.inclusive_ns<<",\"exclusive_ns\":"<<c.exclusive_ns<<'}';};
        for(unsigned k=0;k<26;++k) {if(k) out<<',';out<<'"'<<HotNames[k]<<"\":";counter(counters[k]);}
        out<<"},\"call_sites\":{";
        for(unsigned k=0;k<26;++k) {if(k) out<<',';out<<'"'<<HotNames[k]<<"\":{";
            for(unsigned d=0;d<9;++d) {if(d) out<<',';out<<'"'<<HotDomains[d]<<"\":";counter(sites[k][d]);}out<<'}';}
        out<<"},\"legal_filter_classes\":{";
        for(unsigned i=0;i<15;++i) {if(i)out<<',';const auto& c=legal_classes[i];
            out<<'"'<<LegalClassNames[i]<<"\":{\"generated\":"<<c.generated<<",\"legal\":"<<c.legal<<",\"illegal\":"<<c.illegal<<",\"pushes\":"<<c.pushes<<",\"attacked\":"<<c.attacked<<",\"elapsed_ns\":"<<c.elapsed_ns<<'}';}
        out<<"}}";
    }
};
struct HotScope;
inline thread_local HotProfile* active_hot_profile=nullptr;
inline thread_local HotScope* active_hot_scope=nullptr;
struct LegalSample {
    HotProfile* profile=active_hot_profile;unsigned mask;std::uint64_t attack_start=0;
    std::chrono::steady_clock::time_point start;
    explicit LegalSample(unsigned classes):mask(classes) {
        if(profile){attack_start=profile->counters[static_cast<unsigned>(HotKind::Attack)].calls;start=std::chrono::steady_clock::now();}
    }
    void finish(bool legal,bool pushed) {
        if(!profile)return;
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        const auto attacks=profile->counters[static_cast<unsigned>(HotKind::Attack)].calls-attack_start;
        for(unsigned i=0;i<15;++i)if(mask&(1u<<i)){auto& c=profile->legal_classes[i];++c.generated;c.legal+=legal;c.illegal+=!legal;c.pushes+=pushed;c.attacked+=attacks;c.elapsed_ns+=ns;}
    }
};
struct HotScope {
    HotProfile* profile; HotScope* parent=nullptr; HotDomain previous=HotDomain::Inherit;
    HotKind kind; std::uint64_t children=0;
    std::chrono::steady_clock::time_point start;
    HotScope(HotKind k,HotDomain domain):profile(active_hot_profile),kind(k) {
        if(!profile) return;
        previous=profile->domain;parent=active_hot_scope;active_hot_scope=this;
        if(domain!=HotDomain::Inherit) profile->domain=domain;
        start=std::chrono::steady_clock::now();
    }
    ~HotScope() {
        if(!profile) return;
        auto elapsed=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
        auto update=[&](HotCounter& c){++c.calls;c.inclusive_ns+=elapsed;c.exclusive_ns+=elapsed>=children?elapsed-children:0;};
        update(profile->counters[static_cast<unsigned>(kind)]);update(profile->sites[static_cast<unsigned>(kind)][static_cast<unsigned>(previous)]);
        if(parent) parent->children+=elapsed;
        profile->domain=previous;active_hot_scope=parent;
    }
};
struct HotSession {
    std::unique_ptr<HotProfile> data; HotProfile* previous=active_hot_profile;
    explicit HotSession(bool enabled) {if(enabled) data=std::make_unique<HotProfile>();active_hot_profile=data.get();}
    ~HotSession() {active_hot_profile=previous;}
};
}
#define AXIOM_HOT_JOIN_(a,b) a##b
#define AXIOM_HOT_JOIN(a,b) AXIOM_HOT_JOIN_(a,b)
#define AXIOM_HOT(kind,domain) axiom::HotScope AXIOM_HOT_JOIN(hot_,__LINE__)(axiom::HotKind::kind,axiom::HotDomain::domain)
#define AXIOM_HOT_COPY(bytes) do {if(axiom::active_hot_profile) axiom::active_hot_profile->board_copy_bytes+=(bytes);}while(false)
#else
#define AXIOM_HOT(...) ((void)0)
#define AXIOM_HOT_COPY(...) ((void)0)
#endif
