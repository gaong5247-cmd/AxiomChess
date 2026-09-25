#include "bench.hpp"
#include "tablebase.hpp"
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
namespace ax2 {
namespace {
std::mutex outputMutex;
void emit(const std::string& text){std::lock_guard lock(outputMutex);std::cout<<text<<std::endl;}
std::int64_t number(const std::string& s,std::int64_t low,std::int64_t high){std::size_t end;auto n=std::stoll(s,&end);if(end!=s.size()||n<low||n>high)throw std::invalid_argument("number out of range: "+s);return n;}
std::int64_t read_number(std::istringstream& in,std::int64_t low,std::int64_t high){std::string s;if(!(in>>s))throw std::invalid_argument("missing number");return number(s,low,high);}
std::string score_text(int score){if(std::abs(score)>MateBound)return "mate "+std::to_string((score<0?-1:1)*((Mate-std::abs(score)+1)/2));return "cp "+std::to_string(score);}
void info(const Result& r,int pv){std::ostringstream o;o<<"info depth "<<r.depth<<" seldepth "<<r.seldepth<<" multipv "<<pv<<" score "<<score_text(r.score)<<" nodes "<<r.nodes<<" time "<<r.ms<<" nps "<<r.nodes*1000/std::max<std::int64_t>(1,r.ms)<<" hashfull "<<r.hashfull<<" pv";for(int i=0;i<r.pv.size;++i)o<<' '<<r.pv.moves[i].uci();emit(o.str());}
class UCI {
    Position position;
    std::unique_ptr<Engine> engine=std::make_unique<Engine>();
    Tablebase tb;Features features;
    std::thread worker;std::mutex waitMutex;std::condition_variable wake;
    bool cancel=false,ponder=false,infinite=false;
    int threads=1,multiPV=1,overhead=50;
    bool ponderOption=false;
    void join(){
        {std::lock_guard lock(waitMutex);cancel=true;ponder=false;infinite=false;}
        engine->stop();wake.notify_all();if(worker.joinable())worker.join();
    }
    void set_option(std::istringstream& in){
        join();std::string token,name,value;in>>token;if(token!="name")throw std::invalid_argument("expected option name");
        while(in>>token&&token!="value"){if(!name.empty())name+=' ';name+=token;}std::getline(in,value);if(!value.empty()&&value[0]==' ')value.erase(0,1);
        if(name=="Hash")engine->resize(int(number(value,1,4096)));
        else if(name=="Threads")threads=int(number(value,1,64));
        else if(name=="MultiPV")multiPV=int(number(value,1,256));
        else if(name=="Move Overhead")overhead=int(number(value,0,5000));
        else if(name=="Clear Hash")engine->clear();
        else if(name=="SyzygyPath")emit(tb.open(value)?"info string Syzygy ready":"info string Syzygy unavailable: requires Fathom build and table files");
        else {if(value!="true"&&value!="false")throw std::invalid_argument("expected boolean option");bool on=value=="true";
            if(name=="Ponder")ponderOption=on;
            else if(!features.set(name,on))throw std::invalid_argument("unknown option: "+name);}
    }
    void set_position(std::istringstream& in){
        join();std::string token;in>>token;Position next;
        if(token=="fen"){std::string fen;for(int i=0;i<6;++i){if(!(in>>token))throw std::invalid_argument("incomplete FEN");if(i)fen+=' ';fen+=token;}next.set_fen(fen);}
        else if(token!="startpos")throw std::invalid_argument("expected startpos or fen");
        if(in>>token){if(token!="moves")throw std::invalid_argument("expected moves");while(in>>token){Move m=next.parse_move(token);if(!m)throw std::invalid_argument("illegal move: "+token);
            if(next.historySize>=1900)throw std::invalid_argument("reversible history exceeds capacity");Undo u;next.make(m,u);
            if(next.halfmove==0)next.set_fen(next.fen());
        }}position=next;
    }
    void go(std::istringstream& in){
        join();Limits l;l.overhead=overhead;std::string token;std::vector<std::string> tokens;while(in>>token)tokens.push_back(token);
        auto keyword=[](const std::string& s){return s=="depth"||s=="nodes"||s=="movetime"||s=="wtime"||s=="btime"||s=="winc"||s=="binc"||s=="movestogo"||s=="ponder"||s=="infinite"||s=="mate"||s=="searchmoves";};
        bool explicitLimit=false;
        for(std::size_t i=0;i<tokens.size();++i){token=tokens[i];
            if(token=="ponder"){l.ponder=true;continue;}if(token=="infinite"){l.infinite=true;continue;}
            if(token=="searchmoves"){l.restrictMoves=true;while(i+1<tokens.size()&&!keyword(tokens[i+1])){Move m=position.parse_move(tokens[++i]);if(!m)throw std::invalid_argument("illegal searchmove");if(std::find(l.searchmoves.begin(),l.searchmoves.end(),m)==l.searchmoves.end())l.searchmoves.push_back(m);}continue;}
            if(i+1>=tokens.size())throw std::invalid_argument("missing go value");
            if(token=="nodes"){l.nodes=U64(std::max<std::int64_t>(1,number(tokens[++i],0,9223372036854775807LL)));explicitLimit=true;continue;}
            bool clockValue=token=="wtime"||token=="btime";auto n=number(tokens[++i],clockValue?-2147483647:0,2147483647);
            if(clockValue)n=std::max<std::int64_t>(0,n);
            if(token=="depth"){l.depth=std::clamp(int(n),1,MaxPly-8);explicitLimit=true;}
            else if(token=="movetime"){l.movetime=std::max(1,int(n));explicitLimit=true;}
            else if(token=="wtime")l.time[0]=n;else if(token=="btime")l.time[1]=n;
            else if(token=="winc")l.inc[0]=n;else if(token=="binc")l.inc[1]=n;
            else if(token=="movestogo")l.movestogo=std::max(1,int(n));
            else if(token=="mate"){l.depth=std::clamp(int(std::min<std::int64_t>(n,60))*2,1,MaxPly-8);explicitLimit=true;}
            else throw std::invalid_argument("unknown go token: "+token);
        }
        if(!explicitLimit&&l.time[position.side]<0&&!l.infinite&&!l.ponder)l.movetime=1000;
        {std::lock_guard lock(waitMutex);cancel=false;ponder=l.ponder;infinite=l.infinite;}
        engine->prepare(l,position.side);Position copy=position;
        worker=std::thread([this,copy,l]() mutable {try{
            auto guiLine=[&](Result r){
                // Draw claims are optional in search, but GUI PVs should stop
                // at a claimable position rather than continue past adjudication.
                Position p=copy;
                for(int i=0;i<r.pv.size;++i){Undo u;p.make(r.pv.moves[i],u);if(p.claim_now()||p.automatic_draw()){r.pv.size=i+1;break;}}
                return r;
            };
            auto reporter=[&](const Result& r,int pv){info(guiLine(r),pv);};
            auto r=guiLine(engine->run(copy,l,features,threads,multiPV,reporter,&tb));
            {std::unique_lock lock(waitMutex);wake.wait(lock,[&]{return cancel||(!ponder&&!infinite);});}
            info(r,1);if(copy.can_claim(copy.legal_moves()))emit("info string draw claim available; GUI must adjudicate claim");
            emit("info string stats main_worker "+r.stats.text());std::string best="bestmove "+r.best.uci();if(ponderOption&&r.pv.size>1)best+=" ponder "+r.pv.moves[1].uci();emit(best);
        }catch(const std::exception& e){emit(std::string("info string search error ")+e.what());emit("bestmove 0000");}});
    }
public:
    ~UCI(){join();}
    void loop(){std::string line;while(std::getline(std::cin,line)){std::istringstream in(line);std::string command;in>>command;
        try{
            if(command=="uci"){
                emit("id name AxiomChess 2.0\nid author Axiom contributors\noption name Hash type spin default 32 min 1 max 4096\noption name Threads type spin default 1 min 1 max 64\noption name MultiPV type spin default 1 min 1 max 256\noption name Ponder type check default false\noption name Move Overhead type spin default 50 min 0 max 5000\noption name SyzygyPath type string default <empty>\noption name Clear Hash type button");
                for(auto name:{"LMR","NullMove","Singular","ReverseFutility","Futility","SEEPruning","DeltaPruning"})emit(std::string("option name ")+name+" type check default true");
                for(auto name:{"Razoring","MoveCountPruning","HistoryPruning","ProbCut","DecisionImpact","RefutationSearch"})emit(std::string("option name ")+name+" type check default false");emit("uciok");
            }
            else if(command=="isready")emit("readyok");
            else if(command=="ucinewgame"){join();engine->clear();}
            else if(command=="setoption")set_option(in);
            else if(command=="position")set_position(in);
            else if(command=="go")go(in);
            else if(command=="stop")join();
            else if(command=="ponderhit"){{std::lock_guard lock(waitMutex);if(ponder){engine->ponderhit();ponder=false;}}wake.notify_all();}
            else if(command=="quit"){join();break;}
            else if(command=="bench"){join();std::ostringstream out;bench(out,8,100000,features);emit(out.str());}
            else if(command=="perft"){join();int depth=int(read_number(in,0,8));auto start=now_ms();auto n=perft(position,depth);emit("info string perft nodes "+std::to_string(n)+" time "+std::to_string(now_ms()-start));}
        }catch(const std::exception& e){emit(std::string("info string error ")+e.what());}
    }}
};
}
}
int main(int argc,char** argv){using namespace ax2;try{
    if(argc>1&&std::string(argv[1])=="--bench"){int depth=argc>2?int(number(argv[2],1,120)):8;U64 nodes=argc>3?U64(number(argv[3],0,2147483647)):100000;Features f;for(int i=4;i<argc;++i)if(!f.set(argv[i],true))throw std::invalid_argument("unknown feature");bench(std::cout,depth,nodes,f);}
    else if(argc>1&&std::string(argv[1])=="--perft"){if(argc<3)throw std::invalid_argument("missing perft depth");Position p(argc>3?argv[3]:StartFen);int depth=int(number(argv[2],0,8));auto start=now_ms();auto n=perft(p,depth);std::cout<<"nodes "<<n<<" time_ms "<<now_ms()-start<<'\n';}
    else {auto uci=std::make_unique<UCI>();uci->loop();}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
