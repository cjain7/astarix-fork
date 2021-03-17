#include "align.h"

namespace astarix {

state_t Aligner::readmap(const read_t &r, std::string algo, edge_path_t *best_path) {
    LOG_DEBUG << "Aligning read " << r.comment << ": " << r.s << " of length " << r.len << " using " << algo;

    // Clean up
    read_timers.clear();
    read_timers.total.start();
    read_counters.clear();
    //read_counters.pushed_hist.resize(r.len);
    best_path->clear();

    // Local vars
    path_t p;
    prev_edge_t pe;
#ifndef NDEBUG
    visited_t vis;
#endif
    state_t final_state;    // the best final state; computed in map()
    queue_t Q;              // edge_t(i, u) in Q <=> read[1..i] has been matched with a path ending at u; the prev_state is not used

    assert(G.has_supersource());

    std::string start_suff;

    {
        int i=0, v=0;
        state_t st(0.0, i, v, -1, -1);          // dummy one-after-last state
        push(Q, 0.0, st);                       // to push the next and pop the best
        get_path(p, i, v).optimize(st);         // to hold the optimal values
        set_prev_edge(pe, i, v, edge_t());      // for edge_path reconstruction
    }

    //cost_t prev_cost = 0.0;
    for (int steps=0; !Q.empty(); steps++) {
        LOG_DEBUG << r.comment <<  ": step " << steps << " with best curr sort-cost of " << (double)Q.top().first << ", state=" << Q.top().second;
        
        //prev_cost = Q.top().second.cost;
        state_t curr = pop(Q);
        read_counters.explored_states.inc();

        //LOG_INFO << "node in tree: " << G.node_in_trie(curr.v);
        if (G.node_in_trie(curr.v)) read_counters.popped_trie.inc();
        else read_counters.popped_ref.inc();

        // state (curr.i, curr.v) denotes that the first curr.i-1 characters of the read were already aligned before coming to curr.v. Next to align is curr.i

#ifndef NDEBUG
        // this check is not needed as our heuristic is consistent
        if (visited(vis, curr.i, curr.v)) {  // not correct if a new seed is used
            read_counters.repeated_visits.inc();
            //continue;
        }
        visited(vis, curr.i, curr.v) = true;
#endif

        assert(curr.i <= r.len);
        if (curr.i == r.len) {
            final_state = get_const_path(p, curr.i, curr.v);
            read_timers.total.stop();
            get_best_path_to_state(p, pe, final_state, best_path);
            unique_best = 1;
            if (!Q.empty()) {
                state_t next = pop(Q);
                if (next.i == r.len && EQ(next.cost, curr.cost))
                    unique_best = 0;
            }
            assert(all_read_counters_.find(r.comment) == all_read_counters_.end());
            all_read_counters_[r.comment] = read_counters;
            return final_state;
        }

        // lazy DP / Fast-Forward
        if (params.greedy_match)
            curr = proceed_identity(p, pe, curr, r);

        for (auto it=G.begin_all_matching_edges(curr.v, r.s[curr.i]); it!=G.end_all_matching_edges(); ++it) {
            const edge_t e = *it;
            try_edge(r, curr, p, pe, algo, Q, e);
        }
    }

    assert(false);                                          // no path
    return final_state;                                     // compiler happy
}

void Aligner::try_edge(const read_t &r, const state_t &curr, path_t &p, prev_edge_t &pe, const std::string &algo, queue_t &Q, const edge_t &e) {
    cost_t edge_cost = params.costs.edge2score(e);

    if (e.label != EPS && e.label != r.s[curr.i])
        return;
    
    int i_next = (e.label != EPS) ? curr.i+1 : curr.i;      // maybe move one position in the read
    cost_t g = get_const_path(p, curr.i, curr.v).cost + edge_cost;

    assert(get_const_path(p, curr.i, curr.v).cost < INF);
    assert(g >= 0.0); assert(g < INF);

    state_t next = state_t(g, i_next, e.to, curr.i, curr.v);    

    if (get_path(p, i_next, e.to).optimize(next)) {
        set_prev_edge(pe, i_next, e.to, e);

        read_timers.astar.start();
        cost_t h = astar->h(next);
        cost_t f = g + h;
        read_timers.astar.stop();

        LOG_DEBUG << "From (" << curr.i << ", " << curr.v << ") "
            << "through edge (" << e.label << ", " << edgeType2str(e.type) << ") "
            << "push (" << next.i << ", " << next.v << ") with f=g+h = " << g << " + " << h << " = " << f;
        push(Q, f, next);
    }
}

state_t Aligner::proceed_identity(path_t &p, prev_edge_t &pe, state_t curr, const read_t &r) {
    read_timers.ff.start();

    edge_t e; 
    while (G.numOutOrigEdges(curr.v, &e) == 1 && curr.i < r.len-1 && e.label == r.s[curr.i]) {
        read_counters.greedy_matched.inc();
        state_t next = state_t(curr.cost + params.costs.edge2score(e), curr.i+1, e.to, curr.i, curr.v); 
        if (get_path(p, next.i, next.v).optimize(next)) {
            set_prev_edge(pe, next.i, next.v, e);
        } else {
            read_timers.ff.stop();
            return curr;
        }

        curr = next;
        read_counters.explored_states.inc();
    }

    read_timers.ff.stop();
    return curr;
}

}
