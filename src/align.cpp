#include "align.h"

namespace astarix {

std::vector<state_t> Aligner::readmap(const read_t &r, std::string algo, int max_best_alignments) {
    LOG_DEBUG << "Aligning read " << r.comment << ": " << r.s << " of length " << r.len << " using " << algo;

	assert(max_best_alignments >= 1);

    std::vector<state_t> final_states;  // the best final state; computed in map()
    queue_t Q;              			// edge_t(i, u) in Q <=> read[1..i] has been matched with a path ending at u; the prev_state is not used

    assert(G.has_supersource());

    std::string start_suff;

    {
        pos_t i=0;
		node_t v=0;
        state_t st(0.0, i, v, -1, -1);          // dummy one-after-last state
        push(Q, 0.0, st);                       // to push the next and pop the best
        get_path(p, i, v).optimize(st);         // to hold the optimal values
        set_prev_edge(pe, i, v, edge_t());      // for edge_path reconstruction
    }

    for (int steps=0; !Q.empty(); steps++) {
        LOG_DEBUG << r.comment <<  ": step " << steps << " with best curr sort-cost of " << (double)Q.top().first << ", state=" << Q.top().second;
        
        auto [curr_score, curr_st] = pop(Q);

        if (G.node_in_trie(curr_st.v)) stats.popped_trie.inc();
        else stats.popped_ref.inc();

        // State <curr_st.i, curr_st.v> denotes that the first curr_st.i-1 characters of the read were already aligned before coming to curr_st.v. Next to align is curr_st.i

        if (visited(vis, curr_st.i, curr_st.v)) {
			// Never gets here if the heuristic is consistent.
            stats.repeated_visits.inc();
            continue;
        }
        //visited(vis, curr_st.i, curr_st.v) = true;

        if (curr_score > params.max_align_cost) {  // Drop if the cost gets too high.
            stats.align_status.ambiguous.inc();
            stats.align_status.cost.set( cost_t(0) );
            break;
        }

        assert(curr_st.i <= r.len);
        if (!final_states.empty() && !EQ(final_states.front().cost, curr_st.cost))
            break;
        if (curr_st.i == r.len) {
            state_t final_state = get_const_path(p, curr_st.i, curr_st.v);
            LOG_DEBUG << "Target reached at state <" << curr_st.v << ", " << curr_st.i << "> with cost " << final_state.cost;
            final_states.push_back(final_state);
            stats.align_status.cost.set( final_state.cost );
            if ((int)final_states.size() >= max_best_alignments)
                break;
            else
                continue;
        }

        // lazy DP / Fast-Forward
        if (params.greedy_match)
            curr_st = proceed_identity(p, pe, curr_st, r);

        for (auto it=G.begin_all_matching_edges(curr_st.v, r.s[curr_st.i]); it!=G.end_all_matching_edges(); ++it) {
            const edge_t e = *it;
            try_edge(r, curr_st, p, pe, algo, Q, e);
        }
    }

    if (final_states.empty()) {
        return final_states;
        //CJ: return empty rather than throwing exception
    }

    assert(final_states.size() >= 1 && (int)final_states.size() <= max_best_alignments);
    LOG_DEBUG << final_states.size() << " best alignments of " << r.comment;
    if (final_states.size() > 1)
        stats.align_status.ambiguous.inc();
    else 
        stats.align_status.unique.inc();

    return final_states;
}

void Aligner::try_edge(const read_t &r, const state_t &curr, path_t &p, prev_edge_t &pe, const std::string &algo, queue_t &Q, const edge_t &e) {
    cost_t edge_cost = params.costs.edge2score(e);

    if (e.label != EPS && e.label != r.s[curr.i])
        return;
    
    pos_t i_next = (e.label != EPS) ? curr.i+1 : curr.i;      // Move zero or one positions in the read.
    cost_t g = get_const_path(p, curr.i, curr.v).cost + edge_cost;

    assert(get_const_path(p, curr.i, curr.v).cost < INF);
    assert(g >= 0.0); assert(g < INF);

    state_t next = state_t(g, i_next, e.to, curr.i, curr.v);    

    if (get_path(p, i_next, e.to).optimize(next)) {
        set_prev_edge(pe, i_next, e.to, e);

        stats.t.astar.start();
        cost_t h = astar->h(next);
        stats.t.astar.stop();

        cost_t f = g + h;

        LOG_DEBUG << "From (" << curr.i << ", " << curr.v << ") "
            << "through edge (" << e.label << ", " << edgeType2str(e.type) << ") "
            << "push (" << next.i << ", " << next.v << ") with f=g+h = " << g << " + " << h << " = " << f;
        push(Q, f, next);
    }
}

// Greedy fast-forward exact matching
state_t Aligner::proceed_identity(path_t &p, prev_edge_t &pe, state_t curr, const read_t &r) {
    stats.t.ff.start();

    edge_t e; 
    while (G.numOutOrigEdges(curr.v, &e) == 1 && curr.i < r.len-1 && e.label == r.s[curr.i]) {
        stats.greedy_matched.inc();
        state_t next = state_t(curr.cost + params.costs.edge2score(e), curr.i+1, e.to, curr.i, curr.v); 
        if (get_path(p, next.i, next.v).optimize(next)) {
            set_prev_edge(pe, next.i, next.v, e);
        } else {
            stats.t.ff.stop();
            return curr;
        }

        curr = next;
        stats.explored_states.inc();
    }

    stats.t.ff.stop();
    return curr;
}

}
