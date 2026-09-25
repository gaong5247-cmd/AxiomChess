#include "position.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(){using namespace ax2;try{
    // Fixed manually chosen opening moves for paired arena tests, never used by the engine.
    const char* lines[]={
      "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6",
      "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6",
      "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6",
      "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4",
      "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5",
      "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6",
      "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4",
      "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7",
      "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6",
      "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6",
      "g1f3 d7d5 g2g3 g8f6 f1g2 c7c6",
      "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6"};
    int id=0;for(auto line:lines){Position p;std::istringstream in(line);std::string s;while(in>>s){auto m=p.parse_move(s);if(!m)throw std::runtime_error("invalid opening "+s);Undo u;p.make(m,u);}
        auto fen=p.fen();auto end=fen.rfind(' ');end=fen.rfind(' ',end-1);std::cout<<fen.substr(0,end)<<" id \"axiom2-fixed-"<<++id<<"\";\n";}
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
