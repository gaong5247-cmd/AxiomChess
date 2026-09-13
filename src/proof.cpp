#include "axiom/engine.hpp"
#include <algorithm>

namespace axiom {
MateProof MateSolver::solve(Board b,int attacker,int max_plies,std::uint64_t budget,std::atomic_bool* stop,std::chrono::steady_clock::time_point deadline) {
    nodes_=0; budget_=budget; exhausted_=false; attacker_=attacker; stop_=stop; deadline_=deadline;
    MateProof result; result.attacker=attacker;
    for(int d=0;d<=max_plies;++d) {
        auto cert=visit(b,d);
        if(cert) { result.proven=true; result.distance=cert->distance; result.certificate=cert; break; }
        if(exhausted_) break;
    }
    result.nodes=nodes_; result.exhausted=exhausted_; return result;
}
std::shared_ptr<ProofNode> MateSolver::visit(Board& b,int remaining) {
    if(nodes_>=budget_ || (stop_ && stop_->load()) || std::chrono::steady_clock::now()>=deadline_) { exhausted_=true; return {}; }
    ++nodes_; auto moves=b.legal_moves();
    if(moves.empty()) return b.in_check() && b.side!=attacker_?std::make_shared<ProofNode>():nullptr;
    if(b.automatic_draw() || remaining==0 || (b.side!=attacker_ && b.can_claim_draw())) return {};
    std::stable_sort(moves.begin(),moves.end(),[&](Move a,Move c) {
        auto priority=[&](Move m) { return 100*b.gives_check(m)+10*b.capture(m)+m.promotion; }; return priority(a)>priority(c);
    });
    bool attack=b.side==attacker_; auto node=std::make_shared<ProofNode>();
    for(auto m:moves) {
        auto u=b.push(m); auto child=visit(b,remaining-1); b.pop(u);
        if(exhausted_) return {};
        if(attack && child) { node->edges.push_back({m,child}); node->distance=child->distance+1; return node; }
        if(!attack && !child) return {};
        if(child) { node->edges.push_back({m,child}); node->distance=std::max(node->distance,child->distance+1); }
    }
    return !attack?node:nullptr;
}
bool MateSolver::verify(Board b,int attacker,const std::shared_ptr<ProofNode>& node) {
    if(!node) return false; auto legal=b.legal_moves();
    if(legal.empty()) return b.in_check() && b.side!=attacker && node->edges.empty() && node->distance==0;
    if(b.automatic_draw() || (b.side!=attacker && b.can_claim_draw())) return false;
    if(b.side==attacker ? node->edges.size()!=1 : node->edges.size()!=legal.size()) return false;
    int distance=0; std::vector<Move> seen;
    for(const auto& [m,child]:node->edges) {
        if(std::find(legal.begin(),legal.end(),m)==legal.end() || std::find(seen.begin(),seen.end(),m)!=seen.end()) return false;
        seen.push_back(m); auto u=b.push(m); bool ok=verify(b,attacker,child); b.pop(u);
        if(!ok) return false; distance=std::max(distance,child->distance+1);
    }
    return node->distance==distance;
}
}
