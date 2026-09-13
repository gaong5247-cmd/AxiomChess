#include "axiom/engine.hpp"
#include <algorithm>
namespace axiom {
void Search::order(Board& b,std::vector<Move>& moves,Move tt,int ply) {
    AXIOM_HOT(Order,Order);
    ProfileScope timer(limits_.profile,stats_.order_ns);
    std::vector<RankedMove> overflow;
    if(moves.size()>ordering_scratch_.size()) overflow.resize(moves.size());
    auto* ranked=overflow.empty()?ordering_scratch_.data():overflow.data();
    const bool reuse=limits_.features.reuse_move_facts && move_facts_ && moves.size()<=256 && ply<MaxPly;
    auto* scratch=reuse?move_facts_.get()+MaxPly*256:nullptr;
    { AXIOM_HOT(OrderScoring,Inherit); std::size_t ordinal=0;
    for(auto m:moves) {
        int score=history_score(b,m,ply);
        int exchange=0; bool see_known=false;
        if(m==tt) score+=2000000;
        else if(b.capture(m)) { exchange=see(b,m); see_known=true; score+=(exchange>=0?1000000:-100000)+10*piece_value(b.squares[m.to])-piece_value(b.squares[m.from])+exchange;
            if(limits_.features.capture_history) score+=feedback_->capture_score(b.squares[m.from],m.to,b.squares[m.to]?std::abs(b.squares[m.to]):Pawn); }
        if(m.promotion) score+=800000+piece_value(m.promotion);
        const bool checking=b.gives_check(m);
        if(checking) score+=300000;
        if(ply<MaxPly && (m==killers_[ply][0] || m==killers_[ply][1])) score+=200000;
        if(limits_.features.countermove && m==feedback_->counter(previous_token(ply))) score+=150000;
        if(worker_id_ && m!=tt) score+=(m.from*37+m.to*17+worker_id_*31)%71;
        ranked[ordinal]={score,ordinal,m};
        if(reuse) scratch[ordinal]={m,exchange,see_known,checking};
        ++ordinal;
    }
    }
    const auto better=[](const auto& a,const auto& c){
        return a.score!=c.score?a.score>c.score:a.ordinal<c.ordinal;
    };
    // Experimental top-eight ordering. Tail order changes, so this is not
    // promoted as an equivalent optimization or enabled by default.
    { AXIOM_HOT(OrderSort,Inherit);
    if(limits_.partial_ordering) std::partial_sort(ranked,ranked+std::min<std::size_t>(8,moves.size()),ranked+moves.size(),better);
    else std::sort(ranked,ranked+moves.size(),better);
    }
    for(std::size_t i=0;i<moves.size();++i) {
        moves[i]=ranked[i].move;
        if(reuse) move_facts_[ply*256+i]=scratch[ranked[i].ordinal];
    }
}
}
