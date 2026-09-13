#include "axiom/engine.hpp"
#include "axiom/retrograde.hpp"
#include "axiom/release.hpp"
#include "axiom/protocol.hpp"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

using namespace axiom;
namespace {
std::mutex output_mutex;
void emit(const std::string& s) { std::lock_guard<std::mutex> lock(output_mutex); std::cout<<s<<std::endl; }
std::string score_text(int score) { if(std::abs(score)>MateThreshold) {
    int plies=MateScore-std::abs(score); int moves=(plies+1)/2; return "mate "+std::to_string(score>=0?moves:-moves); }
    return "cp "+std::to_string(score);
}
void info(const SearchResult& r) {
    std::ostringstream o; o<<"info depth "<<r.depth<<" seldepth "<<r.seldepth<<" score "<<score_text(r.score)<<" nodes "<<r.nodes<<" time "<<r.elapsed_ms<<" nps "<<(r.nodes*1000/std::max<long long>(1,r.elapsed_ms))<<" hashfull "<<r.hashfull<<" pv";
    for(const auto& move:r.moves) if(move.move==r.best) for(auto m:move.pv) o<<' '<<m.uci(); emit(o.str());
}
void uci() {
    Board board; auto engine=std::make_unique<Search>(); std::atomic_bool stop=false; std::thread worker;
    std::string syzygy; int proof_plies=5,threads=1; std::uint64_t proof_nodes=0; Features features=production_features(); bool selective=true;
    unsigned safety_mask=SafetyLegacy;
    bool time_guard=false;
    auto join=[&]() { stop=true; if(worker.joinable()) worker.join(); };
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line); std::string command; in>>command;
        try {
            if(command=="uci") {
                emit(std::string("id name ")+EngineIdentity+"\nid author Axiom contributors\noption name Hash type spin default 32 min 1 max 1024\noption name Threads type spin default 1 min 1 max 16\noption name ProofPlies type spin default 5 min 0 max 15\noption name ProofNodes type spin default 0 min 0 max 10000000\noption name SyzygyPath type string default <empty>");
                if constexpr(ResearchBuild) {
                emit("option name Experimental type check default false\noption name Middlegame type check default true");
                Features all; all.all(true); std::istringstream names(all.names()); std::string name;
                emit("option name Selective type check default true");
                emit("option name SafetyMask type string default legacy");
                emit("option name TimeGuard type check default false");
                while(std::getline(names,name,',')) emit("option name Feature_"+name+" type check default "+((","+features.names()+",").find(","+name+",")!=std::string::npos?"true":"false"));
                }
                emit("uciok");
            }
            else if(command=="isready") emit("readyok");
            else if(command=="stop") join();
            else if(command=="quit") { join(); break; }
            else if(command=="ucinewgame") { join(); engine->clear(); }
            else if(command=="setoption") {
                join(); std::string token,name,value; in>>token;
                while(in>>token && token!="value") { if(!name.empty()) name+=' '; name+=token; }
                std::getline(in,value); if(!value.empty() && value.front()==' ') value.erase(value.begin());
                if(!ResearchBuild && name!="Hash" && name!="Threads" && name!="ProofPlies" && name!="ProofNodes" && name!="SyzygyPath")
                    throw std::invalid_argument("Unavailable production option; use research build: "+name);
                if(name=="Hash") { engine=std::make_unique<Search>(std::clamp(std::stoi(value),1,1024)); if(!syzygy.empty()) engine->tablebase.open(syzygy); }
                else if(name=="ProofPlies") proof_plies=std::clamp(std::stoi(value),0,15);
                else if(name=="ProofNodes") proof_nodes=std::clamp<std::uint64_t>(std::stoull(value),0,10000000);
                else if(name=="Threads") threads=std::clamp(std::stoi(value),1,16);
                else if(name=="SafetyMask") safety_mask=parse_safety_mask(value);
                else if(name=="TimeGuard") { if(value!="true" && value!="false") throw std::invalid_argument("Expected boolean"); time_guard=value=="true"; }
                else if(name=="Selective") { if(value!="true" && value!="false") throw std::invalid_argument("Expected true or false"); selective=value=="true"; }
                else if(name=="Middlegame") { if(value=="true") features.middlegame(); else if(value=="false") features.all(false); else throw std::invalid_argument("Expected true or false"); }
                else if(name=="Experimental" || name.starts_with("Feature_")) {
                    if(value!="true" && value!="false") throw std::invalid_argument("Expected true or false");
                    if(name=="Experimental") features.all(value=="true");
                    else if(!features.set(name.substr(8),value=="true")) throw std::invalid_argument("Unknown feature");
                }
                else if(name=="SyzygyPath") { syzygy=value; emit(engine->tablebase.open(value)?"info string Syzygy ready":"info string Syzygy unavailable: build with FATHOM_DIR and provide table files"); }
            } else if(command=="position") {
                join(); std::string kind,token; in>>kind; Board next;
                if(kind=="fen") { std::string fen;
                    for(int i=0;i<6;++i) { if(!(in>>token)) throw std::invalid_argument("Incomplete FEN"); if(i) fen+=' '; fen+=token; } next.set_fen(fen);
                } else if(kind!="startpos") throw std::invalid_argument("Expected startpos or fen");
                if(in>>token) { if(token!="moves") throw std::invalid_argument("Expected moves");
                    while(in>>token) { auto m=next.parse_move(token); if(!m) throw std::invalid_argument("Illegal move: "+token); next.push(*m); }
                } board=next;
            } else if(command=="go" || command=="verify") {
                const auto received=std::chrono::steady_clock::now();
                join(); Limits limits; limits.depth=MaxPly-2; limits.milliseconds=0; limits.mate_plies=proof_plies; limits.proof_nodes=proof_nodes; limits.verify=command=="verify";
                limits.features=features; limits.threads=threads; limits.selective=selective;
                limits.safety_mask=safety_mask;
                limits.time_guard=time_guard;
                int wtime=-1,btime=-1,winc=0,binc=0,movestogo=30; bool infinite=false,explicit_limit=false; std::string token;
                while(in>>token) {
                    if(token=="depth") { limits.depth=static_cast<int>(uci_number(in,token,1,MaxPly-2)); explicit_limit=true; }
                    else if(token=="movetime") { limits.milliseconds=static_cast<int>(uci_number(in,token,1,2147483647)); explicit_limit=true; }
                    else if(token=="nodes") { limits.nodes=static_cast<std::uint64_t>(uci_number(in,token,1,9223372036854775807LL)); explicit_limit=true; }
                    else if(token=="wtime") wtime=static_cast<int>(std::max(0LL,uci_number(in,token,-2147483647,2147483647)));
                    else if(token=="btime") btime=static_cast<int>(std::max(0LL,uci_number(in,token,-2147483647,2147483647)));
                    else if(token=="winc") winc=static_cast<int>(uci_number(in,token,0,2147483647));
                    else if(token=="binc") binc=static_cast<int>(uci_number(in,token,0,2147483647));
                    else if(token=="movestogo") movestogo=static_cast<int>(uci_number(in,token,1,1000000));
                    else if(token=="infinite") infinite=true; else if(token=="verify") limits.verify=true;
                    else if(token=="mate") { int n=static_cast<int>(uci_number(in,token,1,13)); limits.mate_plies=std::clamp(2*n-1,1,25); limits.proof_nodes=1000000; limits.depth=std::max(1,limits.mate_plies); explicit_limit=true; }
                    else if(token=="ponder" || token=="searchmoves") throw std::invalid_argument("Ponder/searchmoves are not supported");
                }
                int remaining=board.side==White?wtime:btime,increment=board.side==White?winc:binc;
                if(remaining>=0 && limits.milliseconds==0) {
                    int available=clock_available(remaining,features.adaptive_time);
                    int allocation=static_cast<int>(std::max(1LL,std::min(static_cast<long long>(available),static_cast<long long>(remaining)/std::max(1,movestogo)+static_cast<long long>(increment)*3/4)));
                    limits.milliseconds=allocation;
                    if(features.time_management) { limits.soft_milliseconds=allocation; limits.milliseconds=std::min(available,allocation>available/3?available:allocation*3); }
                }
                if(!explicit_limit && remaining<0 && !infinite) limits.milliseconds=1000;
                stop=false; Board copy=board;
                worker=std::thread([&,copy,limits,received]() mutable { try {
                    if(limits.milliseconds>0) {
                        const auto queued=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-received).count();
                        limits.queue_ms=queued;
                        limits.milliseconds=static_cast<int>(std::max<long long>(1,limits.milliseconds-queued));
                        if(limits.soft_milliseconds>0) limits.soft_milliseconds=std::min(limits.soft_milliseconds,limits.milliseconds);
                    }
                    auto result=engine->run(copy,limits,stop,info); info(result);
                    emit("info string result "+status_name(result.status)+" outcome "+result.outcome);
                    emit("info string threads "+std::to_string(result.stats.threads)+" correction_updates "+std::to_string(result.stats.correction_updates)+" null_verifications "+std::to_string(result.stats.null_verifications));
                    if(result.claim_draw) emit("info string draw claim available; GUI must adjudicate claim");
                    if(limits.verify) emit("info string verification "+result.verification_detail);
                    emit("bestmove "+result.best.uci());
                } catch(const std::exception& e) { emit(std::string("info string error ")+e.what()); emit("bestmove 0000"); } });
            } else if(command=="bench") { join(); emit("info string bench "+run_bench(4,20000)); }
            else if(command=="d") emit(board.fen());
            else if(command=="eval") { Evaluation e; evaluate_score(board,features.strategic_eval,features.king_safety,nullptr,&e); std::ostringstream out; out<<"info string eval "<<e.total; for(const auto& [n,v]:e.terms) out<<' '<<n<<'='<<v; emit(out.str()); }
            else if(command=="perft") { join(); int depth=1; in>>depth; if(depth<0 || depth>8) throw std::invalid_argument("perft depth must be 0..8"); emit("nodes "+std::to_string(perft(board,depth))); }
        } catch(const std::exception& e) { emit(std::string("info string error ")+e.what()); }
    }
    join();
}
}
int main(int argc,char** argv) {
    try {
        if(argc==1 || std::string(argv[1])=="--uci") { uci(); return 0; }
        std::string mode=argv[1],fen=Board::StartFen,syzygy; Limits limits; int perft_depth=4,attacker=0;
        limits.features=production_features();
        if(mode=="--analyze" || mode=="--bench")limits.proof_nodes=0;
        if(mode=="--bench"){limits.depth=4;limits.nodes=20000;limits.milliseconds=0;}
        for(int i=2;i<argc;++i) { std::string arg=argv[i];
            constexpr const char* research_options[]={"--no-selective","--profile","--correction-mask","--safety-mask","--no-lmr","--no-null","--partial-ordering","--time-guard","--calibration-samples","--no-pvs","--no-tt","--experimental","--feature","--disable-feature","--soft-time","--diagnostics"};
            if(!ResearchBuild && std::find(std::begin(research_options),std::end(research_options),arg)!=std::end(research_options))
                throw std::invalid_argument("Option requires research build: "+arg);
            auto value=[&]() { if(i+1>=argc) throw std::invalid_argument("Missing value for "+arg); return std::string(argv[++i]); };
            if(arg=="--fen") fen=value(); else if(arg=="--depth") limits.depth=std::stoi(value());
            else if(arg=="--time") limits.milliseconds=std::stoi(value()); else if(arg=="--nodes") limits.nodes=std::stoull(value());
            else if(arg=="--plies") limits.mate_plies=std::clamp(std::stoi(value()),0,25); else if(arg=="--proof-nodes") limits.proof_nodes=std::stoull(value());
            else if(arg=="--syzygy") syzygy=value(); else if(arg=="--verify") limits.verify=true;
            else if(arg=="--no-selective") limits.selective=false;
            else if(arg=="--profile") limits.profile=true;
            else if(arg=="--correction-mask") limits.correction_mask=static_cast<unsigned>(std::clamp(std::stoi(value()),0,31));
            else if(arg=="--safety-mask") limits.safety_mask=parse_safety_mask(value());
            else if(arg=="--force-root-move") limits.force_root_move=value();
            else if(arg=="--no-lmr") limits.use_lmr=false;
            else if(arg=="--no-null") limits.use_null=false;
            else if(arg=="--partial-ordering") limits.partial_ordering=true;
            else if(arg=="--time-guard") limits.time_guard=true;
#ifdef AXIOM_RESEARCH_TRACE
            else if(arg=="--trace-search") limits.trace_search=true;
            else if(arg=="--trace-root-move") { limits.trace_root_move=value(); limits.trace_search=true; }
            else if(arg=="--trace-output") limits.trace_output=value();
            else if(arg=="--replay-subtree") limits.replay_subtree=true;
            else if(arg=="--cost-profile") limits.cost_profile=true;
            else if(arg=="--replay-alpha") limits.replay_alpha=std::stoi(value());
            else if(arg=="--replay-beta") limits.replay_beta=std::stoi(value());
            else if(arg=="--trace-min-iteration") { limits.trace_min_iteration=std::stoi(value()); if(limits.trace_min_iteration<0 || limits.trace_min_iteration>=MaxPly) throw std::invalid_argument("Invalid minimum trace iteration"); }
            else if(arg=="--trace-max-events") { auto count=std::stoull(value()); if(count<1 || count>1000000) throw std::invalid_argument("Trace event limit must be 1..1000000"); limits.trace_max_events=static_cast<std::size_t>(count); }
#endif
            else if(arg=="--calibration-samples") limits.calibration_samples=std::clamp(std::stoi(value()),0,4096);
            else if(arg=="--no-pvs") limits.pvs=false;
            else if(arg=="--no-tt") limits.use_tt=false;
            else if(arg=="--experimental") limits.features.all(true);
            else if(arg=="--middlegame") limits.features.middlegame();
            else if(arg=="--feature" || arg=="--disable-feature") { if(!limits.features.set(value(),arg=="--feature")) throw std::invalid_argument("Unknown feature"); }
            else if(arg=="--threads") limits.threads=std::clamp(std::stoi(value()),1,16);
            else if(arg=="--soft-time") limits.soft_milliseconds=std::max(0,std::stoi(value()));
            else if(arg=="--diagnostics") limits.diagnostics=true;
            else if(arg=="--attacker") { auto side=value(); if(side!="white" && side!="black") throw std::invalid_argument("attacker must be white or black"); attacker=side=="white"?White:Black; }
            else if(mode=="--perft" && i==2) perft_depth=std::stoi(arg); else throw std::invalid_argument("Unknown argument: "+arg);
        }
        Board board(fen);
        if(mode=="--bench")std::cout<<run_bench(limits.depth,limits.nodes)<<'\n';
        else if(mode=="--perft") { if(perft_depth<0 || perft_depth>8) throw std::invalid_argument("perft depth must be 0..8"); std::cout<<perft(board,perft_depth)<<'\n'; }
        else if(mode=="--analyze") { Search engine; if(!syzygy.empty() && !engine.tablebase.open(syzygy)) std::cerr<<"Syzygy unavailable; continuing with search\n"; std::atomic_bool stop=false; auto r=engine.run(board,limits,stop); std::cout<<json(r,board); }
        else if(mode=="--prove") { MateSolver solver; std::atomic_bool stop=false;
            auto deadline=limits.milliseconds>0?std::chrono::steady_clock::now()+std::chrono::milliseconds(limits.milliseconds):std::chrono::steady_clock::time_point::max();
            auto p=solver.solve(board,attacker?attacker:board.side,limits.mate_plies,limits.nodes?limits.nodes:limits.proof_nodes,&stop,deadline);
            bool verified=p.proven && MateSolver::verify(board,p.attacker,p.certificate);
            std::cout<<"{\"result\":\""<<(verified?"PROVEN_MATE":"UNKNOWN")<<"\",\"attacker\":\""<<(p.attacker==White?"white":"black")<<"\",\"mate_distance\":"<<p.distance<<",\"nodes\":"<<p.nodes<<",\"certificate_verified\":"<<(verified?"true":"false")<<",\"budget_exhausted\":"<<(p.exhausted?"true":"false")<<"}\n";
        } else if(mode=="--help") std::cout<<"Axiom Chess: UCI engine (no arguments)\n--analyze [--fen FEN] [--depth N] [--time MS (0=unlimited)] [--nodes N] [--verify] [--no-selective]\nSearch feedback: --feature NAME (repeatable), --experimental, --disable-feature NAME\nParallel/time: --threads 1..16 --soft-time MS (requires time_management)\nDebug root patterns: --diagnostics\n--prove [--fen FEN] [--plies N] [--nodes N] [--time MS] [--attacker white|black]\n--perft N [--fen FEN]\nOptional: --syzygy PATH (Fathom-enabled build)\n";
        else throw std::invalid_argument("Unknown mode; use --help");
        return 0;
    } catch(const std::exception& e) { std::cerr<<"error: "<<e.what()<<'\n'; return 1; }
}
