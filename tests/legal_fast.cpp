#include "axiom/chess.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
using namespace axiom;
void verify(Board& b) {
    const auto fen=b.fen(),key=b.proof_key(); const auto hash=b.hash();
    const auto history=b.history; const auto identities=b.identities;
    b.legal_fast_path=true;
    auto fast=b.legal_moves(),old=b.legal_moves_reference();
    if(fast!=old) throw std::runtime_error("Ordered legal mismatch: "+fen);
    if(b.fen()!=fen || b.proof_key()!=key || b.hash()!=hash || b.history!=history || b.identities!=identities)
        throw std::runtime_error("Generation mutated state: "+fen);
}
int main(int argc,char** argv) {
 try {
    const auto target=argc>1?std::stoull(argv[1]):10000ULL;
    std::mt19937_64 rng(2026091307ULL); std::uint64_t positions=0,makes=0,sequences=0;
    const char* fixtures[]={Board::StartFen,
      "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
      "4k3/8/8/r4pPK/8/8/8/8 w - f6 0 1",
      "1r5k/P7/8/8/8/8/8/7K w - - 0 1",
      "4r1k1/8/8/8/1b6/8/8/4K3 w - - 0 1",
      "4r1k1/8/8/8/8/8/4R3/4K3 w - - 99 60",
      "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
      "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"};
    for(auto fen:fixtures) {Board b(fen);verify(b);}
    while(positions<target) {
      Board b(fixtures[sequences%8]); const Board original=b;
      std::vector<Undo> undo;
      for(int ply=0;ply<160 && positions<target;++ply) {
        verify(b);++positions;auto moves=b.legal_moves_reference();if(moves.empty())break;
        undo.push_back(b.push(moves[rng()%moves.size()]));++makes;
      }
      while(!undo.empty()){b.pop(undo.back());undo.pop_back();}
      if(b.squares!=original.squares || b.fen()!=original.fen() || b.hash()!=original.hash() ||
         b.proof_key()!=original.proof_key() || b.history!=original.history || b.identities!=original.identities)
          throw std::runtime_error("Sequence restore mismatch");
      ++sequences;
    }
    Board b;b.legal_fast_path=true;
    if(perft(b,4)!=197281)throw std::runtime_error("Start perft mismatch");
    std::cout<<"PASS legal oracle ordered equivalence positions="<<positions<<" makes="<<makes<<" sequences="<<sequences<<" seed=2026091307\n";
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
