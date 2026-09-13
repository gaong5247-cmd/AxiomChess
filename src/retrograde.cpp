#include "axiom/retrograde.hpp"
#include <queue>
#include <stdexcept>
#include <functional>
#include <algorithm>
namespace axiom {
std::vector<RetroState> retrograde(const std::vector<std::vector<int>>& edges,const std::vector<int>& terminals) {
    const int n=static_cast<int>(edges.size()); if(terminals.size()!=edges.size()) throw std::invalid_argument("graph labels mismatch");
    std::vector<std::vector<int>> predecessors(n); std::vector<int> remaining(n),worst(n); std::vector<bool> solved(n);
    std::vector<RetroState> out(n); using Item=std::pair<int,int>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    for(int i=0;i<n;++i) {
        if(terminals[i]<-1 || terminals[i]>2) throw std::invalid_argument("invalid terminal label");
        if(terminals[i]==2 && edges[i].empty()) throw std::invalid_argument("unlabelled terminal");
        remaining[i]=static_cast<int>(edges[i].size());
        for(int j:edges[i]) { if(j<0 || j>=n) throw std::invalid_argument("invalid successor"); predecessors[j].push_back(i); }
        if(terminals[i]!=2) { solved[i]=true; out[i]={terminals[i],0}; if(terminals[i]) queue.push({0,i}); }
    }
    while(!queue.empty()) { auto [distance,s]=queue.top(); queue.pop();
        for(int p:predecessors[s]) if(!solved[p]) {
            if(out[s].result==-1) { solved[p]=true; out[p]={1,distance+1}; queue.push({distance+1,p}); }
            else { worst[p]=std::max(worst[p],distance+1); if(--remaining[p]==0) { solved[p]=true; out[p]={-1,worst[p]}; queue.push({worst[p],p}); } }
        }
    }
    // Remaining cycles are draws only because the caller supplied a COMPLETE
    // graph with explicit terminal labels; incomplete graphs cannot be solved.
    return out;
}
}
