#include "axiom/engine.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace axiom;
static std::vector<std::string> read_fens(const char* path) {
    std::ifstream input(path); if(!input) throw std::runtime_error("Cannot read FEN file");
    std::vector<std::string> fens; std::string line;
    while(std::getline(input,line)) { if(!line.empty() && line.back()=='\r') line.pop_back(); if(!line.empty() && line[0]!='#') fens.push_back(Board(line).fen()); }
    if(fens.empty()) throw std::runtime_error("Empty FEN file"); return fens;
}
static std::string identity(const std::string& fen) {
    // Exclude root overlap even when move counters differ.
    auto pos=fen.find(' '); for(int i=0;i<3;++i) pos=fen.find(' ',pos+1); return fen.substr(0,pos);
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Usage: axiom_holdout training.fens holdout.fens");
        auto train=read_fens(argv[1]),holdout=read_fens(argv[2]); std::set<std::string> excluded;
        for(const auto& fen:holdout) excluded.insert(identity(fen));
        auto engine=std::make_unique<Search>(32); Limits limits; limits.features.middlegame();
        limits.depth=100; limits.nodes=5000; limits.milliseconds=0; limits.proof_nodes=0;
        std::atomic_bool stop=false; unsigned trained=0;
        for(const auto& fen:train) if(!excluded.contains(identity(fen))) { engine->run(Board(fen),limits,stop); if(++trained==100) break; }
        if(!trained) throw std::runtime_error("No disjoint training roots");
        const auto state=engine->correction_state();
        std::cout<<"{\"training_roots\":"<<trained<<",\"training_nodes_per_root\":5000,\"previous_token\":0,\"state_scaled16\":[";
        for(unsigned i=0;i<state.size();++i) { if(i) std::cout<<','; std::cout<<'[';
            for(unsigned j=0;j<state[i].size();++j) { if(j) std::cout<<','; std::cout<<state[i][j]; } std::cout<<']'; }
        std::cout<<"],\"samples\":[";
        for(unsigned i=0;i<holdout.size();++i) {
            const auto sample=engine->frozen_correction(Board(holdout[i]));
            if(i) std::cout<<',';
            std::cout<<"{\"fen\":\""<<holdout[i]<<"\",\"raw\":"<<sample.raw<<",\"corrected\":"<<sample.corrected<<",\"components_scaled16\":[";
            for(unsigned j=0;j<5;++j) { if(j) std::cout<<','; std::cout<<sample.components[j]; } std::cout<<"]}";
        }
        if(state!=engine->correction_state()) throw std::runtime_error("Holdout modified frozen state");
        std::cout<<"],\"state_unchanged\":true}\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
