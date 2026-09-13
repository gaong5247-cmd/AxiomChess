#include "axiom/san.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace axiom;
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::invalid_argument("Usage: axiom_pgn INPUT.pgn (mainline PGN to JSONL)");
        std::ifstream input(argv[1]); if(!input) throw std::invalid_argument("Cannot open PGN");
        Board b; std::string line; int game=0,ply=0; bool comment=false;
        while(std::getline(input,line)) {
            if(line.starts_with("[Event ")) { ++game; ply=0; b=Board(); comment=false; }
            if(line.starts_with("[FEN \"")) { auto end=line.rfind('"'); b.set_fen(line.substr(6,end-6)); }
            if(line.empty() || line.front()=='[') continue;
            std::string plain;
            for(char c:line) {
                if(c==';' && !comment) break;
                if(c=='{') { comment=true; plain+=' '; }
                else if(c=='}') { comment=false; plain+=' '; }
                else if(!comment) plain+=c;
            }
            std::istringstream words(plain); std::string token;
            while(words>>token) {
                if(token.find('(')!=std::string::npos || token.find(')')!=std::string::npos) throw std::invalid_argument("Variations not supported; supply mainline PGN");
                if(token.front()=='$' || token=="1-0" || token=="0-1" || token=="1/2-1/2" || token=="*") continue;
                if(auto dot=token.rfind('.');dot!=std::string::npos) token.erase(0,dot+1);
                if(token.empty()) continue;
                auto move=parse_san(b,token); auto fen=b.fen(); int pieces=b.piece_count(),reps=b.repetitions(),side=b.side;
                b.push(move); ++ply;
                std::cout<<"{\"game\":"<<game<<",\"ply\":"<<ply<<",\"side\":"<<side<<",\"fen\":\""<<fen<<"\",\"after_fen\":\""<<b.fen()
                    <<"\",\"move\":\""<<move.uci()<<"\",\"phase\":\""<<(pieces<=10?"endgame":"middlegame")<<"\",\"repetitions\":"<<reps<<"}\n";
            }
        }
        if(comment) throw std::invalid_argument("Unclosed PGN comment");
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
