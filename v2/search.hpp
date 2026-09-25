#pragma once
#include "evaluation.hpp"
#include "history.hpp"
#include "time.hpp"
#include "tt.hpp"
#include <functional>
#include <memory>
namespace ax2 {
struct Features {
    bool lmr=true,nullMove=true,singular=true,rfp=true,futility=true,see=true,delta=true;
    bool razor=false,moveCount=false,history=false,probcut=false,decision=false,refutation=false;
    bool set(const std::string& name,bool on);
};
struct Stats {
    U64 qnodes=0,ttHits=0,ttCuts=0,pvs=0,pvsResearch=0,lmr=0,lmrResearch=0,nullTries=0,nullVerify=0,nullReject=0,nullCuts=0;
    U64 singularTries=0,singularExtensions=0,rfp=0,futility=0,see=0,delta=0,razor=0,moveCount=0,history=0,probcutTries=0,probcut=0;
    U64 aspirationLow=0,aspirationHigh=0,decisionProbes=0,decisionNodes=0,refutationProbes=0,refutationNodes=0,refutations=0,tbHits=0;
    U64 nullNodes=0,singularNodes=0,probcutNodes=0,lmrResearchNodes=0;
    U64 decisionInternalProbes=0,decisionInternalNodes=0;
    std::string text() const;
};
struct PV {std::array<Move,MaxPly> moves{};int size=0;};
struct RootMove {
    Move move{};int score=-Inf,depth=0,previous=0,stability=0,pvChurn=0,failures=0,risk=0,uncertainty=0;
    Bound bound=NoBound;U64 work=0;PV pv;
    double priority(int best) const;
};
struct Result {Move best{};int score=0,depth=0,seldepth=0;U64 nodes=0;std::int64_t ms=0;int hashfull=0;PV pv;Stats stats;std::vector<RootMove> roots;};
using Reporter=std::function<void(const Result&,int)>;
struct Stack {Move move{};int token=-1,eval=Inf;PV pv;};
struct Interrupted {};
struct Applied {
    Position& p;Move move;Undo undo;
    Applied(Position& p_,Move m):p(p_),move(m){if(m)p.make(m,undo);else p.make_null(undo);}
    ~Applied(){if(move)p.unmake(move,undo);else p.unmake_null(undo);}
};
class Tablebase;
class Worker {
    friend class Engine;
    TT& tt;Control& control;Tablebase* tb=nullptr;
    Limits limits;Features features;int id=0,seldepth=0;
    U64 localNodes=0;
    std::unique_ptr<Histories> histories=std::make_unique<Histories>();
    RefutationCache refutations;
    std::array<Stack,MaxPly> stack{};
    Stats stats;
    void tick();
    int qsearch(Position& p,int alpha,int beta,int ply,int qply);
    int search(Position& p,int depth,int alpha,int beta,int ply,bool pv,bool nullAllowed=true,Move excluded={},int extensions=0,bool cut=false,bool auxiliary=false);
    void order(const Position& p,MoveList& moves,Move ttMove,int ply,Move refute={});
    void update_pv(int ply,Move m);
    int root_search(Position& p,std::vector<RootMove>& roots,int depth,int alpha,int beta,const std::vector<Move>& excluded);
public:
    Worker(TT& t,Control& c):tt(t),control(c){}
    Result run(Position p,const Limits& l,const Features& f,int workerId,int multiPV,const Reporter& report);
};
class Engine {
    TT tt;
    Control control;
public:
    explicit Engine(int hash=32):tt(hash){}
    void clear(){tt.clear();}
    void resize(int mb){tt.resize(mb);}
    void stop(){control.stop=true;}
    void ponderhit(){control.time.ponderhit();}
    bool pondering()const{return control.time.is_pondering();}
    void prepare(const Limits& l,int side){control.stop=false;control.nodes=0;control.time.reset(l,side);tt.new_search();}
    Result run(const Position& p,const Limits& l,const Features& f,int threads=1,int multiPV=1,const Reporter& report={},Tablebase* tb=nullptr);
};
}
