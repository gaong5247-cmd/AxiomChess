#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace axiom {
enum SafetyReason : unsigned {
    SafetyTT=1u<<0, SafetyPV=1u<<1, SafetyHistory=1u<<2,
    SafetyContinuation=1u<<3, SafetyCounter=1u<<4, SafetyCheck=1u<<5,
    SafetyPromotion=1u<<6, SafetyUnstable=1u<<7, SafetyKing=1u<<8,
    SafetySingular=1u<<9, SafetyOnly=1u<<10, SafetyStrategic=1u<<11,
    SafetyTactical=1u<<12, SafetyImproving=1u<<13
};
constexpr unsigned SafetyAll=(1u<<14)-1;
// Preserve 0.5's gates; the three new protections require separate evidence.
constexpr unsigned SafetyLegacy=SafetyAll & ~(SafetyContinuation|SafetyCounter|SafetyKing);
constexpr std::array<std::string_view,14> SafetyNames={"tt","pv","history","continuation","counter","check","promotion","unstable","king","singular","onlymove","strategic","tactical","improving"};
inline unsigned parse_safety_mask(const std::string& text) {
    if(text=="all") return SafetyAll;
    if(text=="legacy") return SafetyLegacy;
    if(text=="none") return 0;
    if(text=="no-trend") return SafetyAll & ~(SafetyUnstable|SafetyImproving);
    if(text.starts_with("no-")) {
        for(unsigned i=0;i<SafetyNames.size();++i)
            if(text.substr(3)==SafetyNames[i]) return SafetyAll & ~(1u<<i);
        throw std::invalid_argument("Unknown safety reason: "+text);
    }
    if(text.empty() || text.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("Invalid safety mask: "+text);
    auto value=std::stoul(text);
    if(value>SafetyAll) throw std::invalid_argument("Safety mask outside 0..16383");
    return static_cast<unsigned>(value);
}
enum class SafetyAction : unsigned { Lmr, History, See, Futility, Rfp, Null, Probcut, Singular, Count };
constexpr std::array<std::string_view,8> SafetyActionNames={"lmr","history","see","futility","rfp","null","probcut","singular"};
struct SafetyStats {
    // Joint credit: a move with two reasons increments both. Not causal node savings.
    std::array<std::array<std::uint64_t,14>,8> skipped_checks{};
    void record(SafetyAction action,unsigned reasons) {
        for(unsigned i=0;i<14;++i) if(reasons & (1u<<i)) ++skipped_checks[static_cast<unsigned>(action)][i];
    }
    void add(const SafetyStats& other) {
        for(unsigned a=0;a<8;++a) for(unsigned r=0;r<14;++r) skipped_checks[a][r]+=other.skipped_checks[a][r];
    }
};
}
