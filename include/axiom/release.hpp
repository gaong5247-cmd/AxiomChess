#pragma once
#include "axiom/engine.hpp"
namespace axiom {
#ifdef AXIOM_RESEARCH_TRACE
inline constexpr bool ResearchBuild=true;
inline constexpr const char* EngineIdentity="Axiom Chess 1.0-rc1 research";
#else
inline constexpr bool ResearchBuild=false;
inline constexpr const char* EngineIdentity="Axiom Chess 1.0-rc1";
#endif
// Frozen playing policy: the previously tested Middlegame configuration.
// Candidate speedups and rejected search/time experiments are NOT promoted.
inline Features production_features() { Features f;f.middlegame();return f; }
std::string run_bench(int depth,std::uint64_t nodes);
}
