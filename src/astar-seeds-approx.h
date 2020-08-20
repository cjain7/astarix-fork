#pragma once

#include <map>
#include <string>
#include <vector>

#include "graph.h"
#include "utils.h"
#include "io.h"

typedef int node_t;

namespace astarix {

class AStarSeedsWithErrors: public AStarHeuristic {
  private:
    // Fixed parameters
    const graph_t &G;
    const EditCosts &costs;
    const arguments::AStarSeedsArgs args;

    // Updated separately for every read
    const read_t *r_;
    const int shifts_allowed_;  // number of deletions accomodated in first trie_depth nucleotides

    // H[u] := number of exactly aligned seeds after node `u`
    // It is safe to increase more to H than needed.
    // TODO: make H dependent on the distance to the seed
    static const int MAX_SEED_ERRORS = 3;
    std::vector<int> H[MAX_SEED_ERRORS+1];
//    std::vector<int> visited;  // == +1 if a node has already been added to H; -1 if a node has already been subtracted from H
    //std::unordered_map<node_t, cost_t> _star;

    // stats per read
    int seeds;          // number of seeds (depends only on the read)
    int seed_matches;   // places in the graph where seeds match well enough
    int paths_considered;  // number of paths updated for all seeds (supersource --> match)
    int marked_states;     // the number of states marked over all paths for all seeds

    // global stats
    int reads_;            // reads processed
    int seeds_;          
    int seed_matches_;   
    int paths_considered_;  
    int marked_states_;     
    cost_t best_heuristic_sum_;

  public:
    AStarSeedsWithErrors(const graph_t &_G, const EditCosts &_costs, arguments::AStarSeedsArgs _args, int _shifts_allowed)
        : G(_G), costs(_costs), args(_args), shifts_allowed_(_shifts_allowed) {
        for (int i=0; i<=args.max_seed_errors; i++)
            H[i].resize(G.nodes());
        reads_ = 0;
        seeds_ = 0;
        seed_matches_ = 0;
        paths_considered_ = 0;
        marked_states_ = 0;
        best_heuristic_sum_ = 0;
        //LOG_INFO << "A* matching class constructed with:";
        //LOG_INFO << "  seed_len    = " << args.seed_len;
    }

    // assume only hamming distance (substitutions)
    // every seed is a a pair (s,j), s.t. s=r[j...j+seed_len)
    //                              j2       j1       j0
    // r divided into seeds: ----|---s2---|---s1---|---s0---|
    // alignment:                u  v2       v1       v0
    //
    // h(<u,i>) := P - f(<u,i>)
    // f(<u,i>) := |{ (s,j) \in seed | exists v: exists path from u->v of length exactly (j-i) and s aligns exactly from v }|,
    // 
    // where P is the number of seeds
    // Accounts only for the last seeds.
    // O(1)
    cost_t h(const state_t &st) const {
        int all_seeds_to_end = (r_->len - st.i - 1) / args.seed_len;

        int total_errors = (args.max_seed_errors+1)*all_seeds_to_end;  // the maximum number of errors
        int not_used_mask = ((1<<all_seeds_to_end)-1);   // at first no seed is used: 111...11111 (in binary)
        for (int errors=0; errors<=args.max_seed_errors; errors++) {
            int h_remaining = H[errors][st.v] & not_used_mask;
            int matching_seeds = __builtin_popcount(h_remaining);
            assert(matching_seeds <= all_seeds_to_end);
            not_used_mask &= ~h_remaining;  // remove the bits for used seeds

            total_errors -= matching_seeds*(args.max_seed_errors+1-errors); // for 0 errors, lower the heuristic by max_seed_errors+1
        }
        
        cost_t res = total_errors * costs.get_min_mismatch_cost();
        //LOG_DEBUG << "h(" << st.i << ", " << st.v << ") = "
        //          << total_errors << " * " << costs.get_min_mismatch_cost()
        //          << " = " << res;
        return res;
    }

    // Cut r into chunks of length seed_len, starting from the end.
    void before_every_alignment(const read_t *r) {
        ++reads_;
        paths_considered = 0;
        marked_states = 0;
        seed_matches = 0;

        r_ = r;
        seeds = gen_seeds_and_update(r, +1);

        seeds_ += seeds;
        seed_matches_ += seed_matches;
        paths_considered_ += paths_considered;
        marked_states_ += marked_states;
        best_heuristic_sum_ += h(state_t(0.0, 0, 0, -1, -1));

        LOG_INFO << r->comment << " A* seeds stats: "
                 << seeds << " seeds " 
                 << "matching at " << seed_matches << " graph positions "
                 << "and generating " << paths_considered << " paths "
                 << "over " << marked_states << " states"
                 << "with best heuristic " << h(state_t(0.0, 0, 0, -1, -1)) << " "
                 << "out of possible " << (args.max_seed_errors+1)*seeds;
//                 << "which compensates for <= " << (args.max_seed_errors+1)*seeds - h(state_t(0.0, 0, 0, -1, -1)) << " errors";
    }

    void after_every_alignment() {
        // Revert the updates by adding -1 instead of +1.
        gen_seeds_and_update(r_, -1);

        // TODO: removedebug
        for(int e=0; e<MAX_SEED_ERRORS; e++)
            for(int i=0; i<H[e].size(); i++)
                assert(H[e][i] == 0);
    }

    void print_params(std::ostream &out) const {
        out << "     seed length: " << args.seed_len << " bp"                    << std::endl;
        out << " max seed errors: " << args.max_seed_errors                      << std::endl;
        out << "     shifts allowed: " << shifts_allowed_                           << std::endl;
    }

    void print_stats(std::ostream &out) const {
        out << "        For all reads:"                                             << std::endl;
        out << "                            Seeds: " << seeds_                << std::endl;
        out << "                     Seed matches: " << seed_matches_
            << "(" << 1.0*seed_matches_/reads_ << " per read)"                   << std::endl;
        out << "                    Paths considered: " << paths_considered_        << std::endl;
        out << "                  Graph nodes marked: " << marked_states_           << std::endl;
        out << "                Best heuristic (avg): " << (double)best_heuristic_sum_/reads_ << std::endl;
//        out << "                 Max heursitic value: " << ?? << std::endl;
    }

  private:
    // Proceed if node is in the trie and it is time for the trie,
    //   or if the node is not in the trie and it is too early for the trie.
    // trie_depth is the number of node levels (including the supersource)
    //bool should_proceed_backwards_to(int i, node_t v) {
    //    bool time_for_trie = i < G.get_trie_depth();
    //    bool node_in_trie = G.node_in_trie(v);
    //    //LOG_DEBUG << "next node: " << v << ", time_for_trie: " << time_for_trie << ", node_in_trie: " << node_in_trie;
    //    return !(time_for_trie ^ node_in_trie);
    //}

    int diff(int a, int b) { return abs(a-b); }

    // `H[u]+=dval` for all nodes (incl. `v`) that lead from `0` to `v` with a path of length `i`
    // TODO: optimize with string nodes
    // Fully ignores labels.
    // Returns if the the supersource was reached at least once.
    bool update_path_backwards(int p, int i, node_t v, int dval, int shifts_remaining, int errors) {
        LOG_DEBUG_IF(dval == +1) << "Backwards trace: (" << i << ", " << v << ")";
        if (dval == +1) {
            if (!(H[errors][v] & (1<<p))) {
                ++marked_states;
                H[errors][v] |= 1<<p;   // fire p-th bit
            }
        } else {
            H[errors][v] &= ~(1<<p);  // remove p-th bit
        }
        //LOG_DEBUG_IF(dval == +1) << "H[" << errors << "][" << v << "] = " << H[errors][v];
        //assert(__builtin_popcount(H[errors][v]) <= seeds);

        if (v == 0) {
//            assert(i == 0);  // supersource is reached; no need to update H[0]
            ++paths_considered;
            return true;
        }

        bool at_least_one_path = false;
        for (auto it=G.begin_orig_rev_edges(v); it!=G.end_orig_rev_edges(); ++it) {
            // TODO: iterate edges from the trie separately from the edges from the graph
            LOG_DEBUG_IF(dval == +1) << "Traverse the reverse edge " << v << "->" << it->to << " with label " << it->label;
            if ( (G.node_in_trie(v))  // (1) already in trie
              || (diff(i-1, G.get_trie_depth()) <= shifts_remaining)  // (2) time to go to trie
              || (i-1 > G.get_trie_depth() && !G.node_in_trie(it->to))) {  // (3) proceed back
                bool success = update_path_backwards(p, i-1, it->to, dval, shifts_remaining, errors);
                if (success) at_least_one_path = true;
            }
        }

        return at_least_one_path;
    }

    // Assumes that seed_len <= D so there are no duplicating outgoing labels.
    // In case of success, v is the 
    // Returns a list of (shift_from_start, v) with matches
    void match_seed_and_update(const read_t *r, int p, int start, int i, node_t v, int dval, int remaining_errors) {
        //LOG_DEBUG_IF(dval == +1) << "Match forward seed " << p << "[" << start << ", " << start+seed_len << ") to state (" << i << ", " << v << ")"
        //                         << " with " << remaining_errors << " remaining errors.";
        if (i < start + args.seed_len) {
            // TODO: match without recursion if unitig
            // Match exactly down the trie and then through the original graph.
            for (auto it=G.begin_all_matching_edges(v, r->s[i]); it!=G.end_all_edges(); ++it) {
                int new_remaining_errors = remaining_errors;
                int new_i = i;
                //LOG_DEBUG << "edge: " << it->label << ", " << edgeType2str(it->type);
                if (it->type != DEL)
                    ++new_i;
                if (it->type != ORIG && it->type != JUMP)  // ORIG in the graph, JUMP in the trie
                    --new_remaining_errors;
                if (new_remaining_errors >= 0)
                    match_seed_and_update(r, p, start, new_i, it->to, dval, new_remaining_errors);
            }
//        } else if (G.node_in_trie(v)) {
//            // Climb the trie.
//            for (auto it=G.begin_orig_edges(v); it!=G.end_orig_edges(); ++it)
//                match_seed_and_update(r, start, i+1, it->to, dval);
        } else {
            assert(!G.node_in_trie(v));
            //LOG_INFO_IF(dval == +1) << "Updating for seed " << p << "(" << i << ", " << v << ") with dval=" << dval << " with " << max_seed_errors-remaining_errors << " errrors.";
            bool success = update_path_backwards(p, i, v, dval, shifts_allowed_, args.max_seed_errors-remaining_errors);
            assert(success);

            ++seed_matches;  // debug info
        }
    }

    // Split r into seeds of length seed_len.
    // For each exact occurence of a seed (i,v) in the graph,
    //   add dval to H[u] for all nodes u on the path of match-length exactly `i` from supersource `0` to `v`
    // Returns the number of seeds.
    int gen_seeds_and_update(const read_t *r, int dval) {
        int seeds = 0;
        for (int i=r->len-args.seed_len; i>=0; i-=args.seed_len) {
            // seed from [i, i+seed_len)
            match_seed_and_update(r, seeds, i, i, 0, dval, args.max_seed_errors);
            seeds++;
        }
        return seeds;
    }
};

}
