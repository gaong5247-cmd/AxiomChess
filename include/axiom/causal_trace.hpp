#pragma once
#ifdef AXIOM_RESEARCH_TRACE
#include "axiom/chess.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace axiom {
struct CausalEvent {
    std::string reason,fen,root_move,move,tt_bound;
    std::uint64_t event_id=0,node_id=0,parent_id=0,nodes=0,hash=0;
    int ply=0,depth=0,qply=-1,side=0,alpha=0,beta=0,index=-1,reduction=0;
    unsigned safety=0;
    std::optional<int> score,raw,corrected,history,continuation,capture_history,see,tt_score,tt_depth,trend;
    std::optional<std::array<int,5>> correction;
    bool capture=false,check=false,promotion=false,counter=false,pv=false,synthetic=false,auxiliary=false;
    double elapsed_ms=0;
};
inline std::string trace_quote(const std::string& s) {
    std::string out="\""; for(unsigned char c:s) {
        if(c=='"' || c=='\\') {out+='\\';out+=static_cast<char>(c);}
        else if(c=='\n') out+="\\n";
        else if(c>=32) out+=static_cast<char>(c);
    } return out+'"';
}
struct CausalTrace {
    std::vector<CausalEvent> events;
    std::uint64_t active=0,next_node=0,dropped=0;
    std::size_t cap=100000;
    std::size_t regular_events=0;
    int iteration_depth=0,min_iteration=0;
    std::string root_filter;
    std::chrono::steady_clock::time_point start;
    bool accepts(const std::string& root) const { return iteration_depth>=min_iteration && (root_filter.empty() || root==root_filter); }
    bool full() const { return regular_events>=cap-std::min<std::size_t>(cap/4,4096); }
    void emit(CausalEvent event,std::uint64_t nodes) {
        bool root_metadata=event.reason=="ROOT_ITERATION_RANK";
        if(events.size()>=cap || (!root_metadata && full())) { ++dropped; return; }
        if(!root_metadata) ++regular_events;
        event.nodes=nodes; event.event_id=events.size()+1;
        event.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        events.push_back(std::move(event));
    }
    void save(const std::string& path) const {
        // Caller reserves the destination before search; output is published once.
        std::ofstream out(path,std::ios::app); if(!out) throw std::runtime_error("Cannot write trace");
        for(const auto& e:events) {
            out<<"{\"event_id\":"<<e.event_id<<",\"node_id\":"<<e.node_id<<",\"parent_id\":"<<e.parent_id
               <<",\"reason\":"<<trace_quote(e.reason)<<",\"fen\":"<<trace_quote(e.fen)<<",\"hash\":"<<trace_quote(std::to_string(e.hash))
               <<",\"root_move\":"<<trace_quote(e.root_move)<<",\"move\":"<<trace_quote(e.move)<<",\"ply\":"<<e.ply<<",\"depth\":"<<e.depth
               <<",\"qply\":"<<e.qply<<",\"side\":"<<e.side<<",\"alpha\":"<<e.alpha<<",\"beta\":"<<e.beta
               <<",\"index\":"<<e.index<<",\"reduction\":"<<e.reduction<<",\"safety_reasons\":"<<e.safety
               <<",\"tt_bound\":"<<trace_quote(e.tt_bound)<<",\"nodes\":"<<e.nodes<<",\"elapsed_ms\":"<<e.elapsed_ms;
#define TRACE_OPTIONAL(field) out<<",\"" #field "\":"; if(e.field) out<<*e.field; else out<<"null";
            TRACE_OPTIONAL(score) TRACE_OPTIONAL(raw) TRACE_OPTIONAL(corrected) TRACE_OPTIONAL(history)
            TRACE_OPTIONAL(continuation) TRACE_OPTIONAL(capture_history) TRACE_OPTIONAL(see)
            TRACE_OPTIONAL(tt_score) TRACE_OPTIONAL(tt_depth) TRACE_OPTIONAL(trend)
#undef TRACE_OPTIONAL
#define TRACE_BOOL(field) out<<",\"" #field "\":"<<(e.field?"true":"false");
            TRACE_BOOL(capture) TRACE_BOOL(check) TRACE_BOOL(promotion) TRACE_BOOL(counter)
            TRACE_BOOL(pv) TRACE_BOOL(synthetic) TRACE_BOOL(auxiliary)
#undef TRACE_BOOL
            out<<",\"correction_scaled16\":";
            if(e.correction) {out<<'[';for(unsigned i=0;i<5;++i) {if(i) out<<',';out<<(*e.correction)[i];} out<<']';} else out<<"null";
            out<<"}\n";
        }
        out<<"{\"reason\":\"TRACE_SUMMARY\",\"events\":"<<events.size()<<",\"dropped\":"<<dropped<<",\"complete\":"<<(dropped?"false":"true")<<"}\n";
        if(!out) throw std::runtime_error("Trace write failed");
    }
};
struct CausalScope {
    CausalTrace* sink=nullptr; CausalEvent base; std::uint64_t previous=0;
    CausalScope(CausalTrace* trace,const Board& b,int depth,int alpha,int beta,int ply,int qply,
                const std::string& root,bool pv,bool synthetic,bool auxiliary,std::uint64_t nodes) {
        if(!trace || !trace->accepts(root)) return;
        if(trace->full()) { ++trace->dropped; return; }
        sink=trace; previous=sink->active; base.node_id=++sink->next_node; base.parent_id=previous;
        sink->active=base.node_id; base.fen=b.fen(); base.hash=b.hash(); base.side=b.side;
        base.depth=depth;base.alpha=alpha;base.beta=beta;base.ply=ply;base.qply=qply;base.root_move=root;
        base.pv=pv;base.synthetic=synthetic;base.auxiliary=auxiliary;
        event("NODE_ENTER",nodes);
    }
    ~CausalScope() { if(sink) sink->active=previous; }
    void event(const char* reason,std::uint64_t nodes,Move move={},std::optional<int> score={}) {
        if(!sink) return; auto e=base;e.reason=reason;e.move=move?move.uci():"";e.score=score;sink->emit(std::move(e),nodes);
    }
};
}
#define AXIOM_TRACE_SCOPE(...) axiom::CausalScope causal(__VA_ARGS__)
#define AXIOM_TRACE_EVENT(reason,move,score) causal.event(reason,nodes_,move,score)
#define AXIOM_TRACE_DETAIL(code) do { if(causal.sink) { code; } } while(false)
#else
#define AXIOM_TRACE_SCOPE(...) ((void)0)
#define AXIOM_TRACE_EVENT(...) ((void)0)
#define AXIOM_TRACE_DETAIL(...) ((void)0)
#endif
