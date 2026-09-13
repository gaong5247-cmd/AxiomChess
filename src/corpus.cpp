#include "axiom/engine.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <filesystem>
#include <algorithm>

using namespace axiom;
int main(int argc,char** argv) {
    try {
        if(argc<2) throw std::invalid_argument("Usage: axiom_corpus OUTPUT.fens [count=1000] [seed=20260912]");
        int count=argc>2?std::stoi(argv[2]):1000;
        if(count<1 || count>5000) throw std::invalid_argument("count must be 1..5000");
        if(std::filesystem::exists(argv[1])) throw std::invalid_argument("Output exists; refusing overwrite");
        std::mt19937 rng(argc>3?static_cast<unsigned>(std::stoul(argv[3])):20260912u);
        std::set<std::string> unique;
        for(int game=0;game<count*50 && static_cast<int>(unique.size())<count;++game) {
            Board b;
            for(int ply=0;ply<65;++ply) {
                auto legal=b.legal_moves(); if(legal.empty() || b.automatic_draw()) break;
                // Mix plausible static choices with random legal continuations.
                // These are candidate test positions, NOT labelled errors.
                Move chosen=legal[rng()%legal.size()];
                if(rng()%4!=0) {
                    int best=-Infinity;
                    for(auto move:legal) { auto undo=b.push(move,false);
                        int value=-evaluate_score(b)+int(rng()%121)-60; b.pop(undo);
                        if(value>best) { best=value; chosen=move; }
                    }
                }
                b.push(chosen);
                if(ply>=15 && ply<=55 && ply%4==(game%2?2:3) && b.piece_count()>=16 && !b.in_check()) {
                    unique.insert(b.fen()); if(static_cast<int>(unique.size())>=count) break;
                }
            }
        }
        if(static_cast<int>(unique.size())<count) throw std::runtime_error("Could not generate enough unique positions");
        std::ofstream out(argv[1]); if(!out) throw std::runtime_error("Cannot open output");
        out<<"# Deterministic generated middlegame candidates, not verified mistakes.\n";
        std::vector<std::string> shuffled(unique.begin(),unique.end());
        std::shuffle(shuffled.begin(),shuffled.end(),rng);
        for(const auto& fen:shuffled) out<<fen<<'\n';
        std::cout<<"Generated "<<unique.size()<<" candidate FENs\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
