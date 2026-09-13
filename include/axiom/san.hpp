#pragma once
#include "axiom/chess.hpp"
#include <algorithm>
#include <stdexcept>
namespace axiom {
inline Move parse_san(Board& b,std::string san) {
    while(!san.empty() && std::string("+#!?").find(san.back())!=std::string::npos) san.pop_back();
    if(auto uci=b.parse_move(san)) return *uci;
    if(san=="O-O" || san=="0-0" || san=="O-O-O" || san=="0-0-0") {
        const bool short_castle=san.size()==3;
        auto move=b.parse_move(std::string(b.side==White?"e1":"e8")+(b.side==White?(short_castle?"g1":"c1"):(short_castle?"g8":"c8")));
        if(move) return *move; throw std::invalid_argument("Illegal castling SAN");
    }
    auto type=[](char c) { auto p=std::string(" PNBRQK").find(c); return p==std::string::npos?0:static_cast<int>(p); };
    int promotion=0; auto equals=san.find('=');
    if(equals!=std::string::npos) { if(equals+2!=san.size()) throw std::invalid_argument("Invalid promotion SAN"); promotion=type(san.back()); san.resize(equals); if(promotion<Knight || promotion>Queen) throw std::invalid_argument("Invalid promotion piece"); }
    if(san.size()<2) throw std::invalid_argument("Invalid SAN");
    int target=parse_square(san.substr(san.size()-2)); san.resize(san.size()-2);
    int piece=Pawn; if(!san.empty() && type(san.front())) { piece=type(san.front()); san.erase(san.begin()); }
    const bool capture=san.find('x')!=std::string::npos;
    san.erase(std::remove(san.begin(),san.end(),'x'),san.end());
    Move chosen; int matches=0;
    for(auto m:b.legal_moves()) if(m.to==target && m.promotion==promotion && std::abs(b.squares[m.from])==piece && b.capture(m)==capture) {
        const auto from=square_name(m.from); bool fits=true;
        for(char c:san) if(c!=from[0] && c!=from[1]) fits=false;
        if(fits) { chosen=m; ++matches; }
    }
    if(matches!=1) throw std::invalid_argument("Illegal or ambiguous SAN");
    return chosen;
}
}
