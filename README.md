<img width="100" alt="AStarix" align="left" src="https://www.sri.inf.ethz.ch/assets/systems/astarix.png"><br/>
<a href="https://www.sri.inf.ethz.ch/"><img width="100" alt="SRILAB" align="right" src="http://safeai.ethz.ch/img/sri-logo.svg"></a><br/>

---

![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)

**AStarix** is a sequence-to-graph aligner based on A\* shortest path algorithm.
It supports general graphs and finds alignments that are optimal according to edit-distance with non-negative weights.
Currently, it uses the Seed heuristic.

A [preprint](https://www.biorxiv.org/content/10.1101/2020.01.22.915496v1) using AStarix has been accepted to [RECOMB2020](https://www.recomb2020.org/). Evaluation scripts are available in the [RECOMB2020_experiments branch](https://github.com/eth-sri/astarix/tree/RECOMB2020_experiments/evals).

## Installation

Alternatively to using Docker, you can install `AStarix` directly on your system.

### Prerequisites

In order to compile `AStarix`, you will need to first install:

* [argp](https://www.gnu.org/software/libc/manual/html_node/Argp.html) &ndash;
  argument parsing library
* (optional) [pandas](https://pandas.pydata.org/) &ndash; dataframe library used for testing and evalutaions

We show how to install them in the [Dockerfile](./Dockerfile).
Other third-party libraries are located in the `/ext` directory and their own licenses apply.

### Compile and test

```bash
make
make test
```

# Example run

This is an example run of `AStarix` on a toy data: aligning 100 simulated Illumina reads to a linear graph from the first 10000bp of Escherichia coli. You can expect a similar summary printed to standard output:

```
$ bin/astarix align-optimal -g data/ecoli_head10000_linear.gfa -q data/illumina.fq -o tmp/astar-default
Reading queries...
Precomputation...
Aligning...

       == Input ==
              Queries/reads: 100 read in 0.00s
Graph nodes, edges, density: 50146, 69774, 0
           Reading run time: 0.00s
       == General parameters and optimizations == 
             Alignment algo: astar-prefix
                 Edit costs: 0.00, 1.00, 5.00, 5.00 (match, subst, ins, del)
                    Threads: 1
              Greedy match?: true
       == Aligning statistics ==
                      Reads: 100 x 100bp
          Aligning run time: 0.10s (A*: 30.10%, que: 6.03%, dicts: 20.95%, greedy_match: 17.34%)
                Performance: 1049.68 reads/s, 102.51 Kbp/s
   Explored rate (avg, max): 0.89, 2.41    [states/bp] (states normalized by query length)
     Expand rate (avg, max): 0.13, 0.31
       == A* parameters ==
                   Cost cap: 5.00
   Upcoming seq. length cap: 5
      Nodes equiv. classes?: true
  Compressible equiv. nodes: 37049 (73.88%)
    Precomputation run time: 0.03s
                     Memory: 0.11 GB (graph: 0.91%, A*-memoization: <0.01% for 8444 entries)
       Memoization hit rate: 3.50%
```

The directory `output_dir/` will contain a file with execution logs and a file with alignment statistics.
Short aggregated statistics are print to standard output (to redirect, you can add `>output_dir/summary.txt`).


# Usage

`AStarix` currently works only in exact/optimal mode (specified using `align-optimal`). Currently supported formats are `.gfa` without overlapping nodes (for a graph reference) and `.fa`/`.fasta` (for a linear reference). The queries should be in `.fq`/`.fastq` format (the phred values are ignored).
Tested on Ubuntu 18.04.

```
$ ./bin/astarix --help
Usage: astarix [OPTION...] align-optimal -g GRAPH.gfa -q READS.fq -o OUT_DIR/
Optimal sequence-to-graph aligner based on A* shortest path.

  -a, --algorithm={dijkstra, astar-prefix}
                             Shortest path algorithm
  -c, --astar_cost_cap=A*_COST_CAP
                             The maximum prefix cost for the A* heuristic
  -d, --astar_len_cap=A*_PREFIX_CAP
                             The upcoming sequence length cap for the A*
                             heuristic
  -D, --tree_depth=TREE_DEPTH   Suffix tree depth
  -e, --astar_equivalence_classes=A*_EQ_CLASSES
                             Whether to partition all nodes to equivalence
                             classes in order not to reuse the heuristic
  -f, --greedy_match=GREEDY_MATCH
                             Proceed greedily forward if there is a unique
                             matching outgoing edge
  -g, --graph=GRAPH          Input graph (.gfa)
  -G, --gap=GAP_COST         Gap (Insertion or Deletion) penalty
  -M, --match=MATCH_COST     Match penalty
  -o, --outdir=OUTDIR        Output directory
  -q, --query=QUERY          Input queries/reads (.fq, .fastq)
  -S, --subst=SUBST_COST     Substitution penalty
  -t, --threads=THREADS      Number of threads (default=1)
  -v, --verbose=THREADS      Verbosity (default=silent=0, info=1, debug=2)
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```

# Technical details

`AStarix` computes an A* heuristic function __h__ that directs the path search faster by anticipating the upcoming nucleotides to be aligned.
Once computed, a value of __h__ is memoized in a hash table ([parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap)).

`AStarix` parallelizes the alignment of a set of reads.

# Historical note
The development of the `AStarix` algorithm is only possible because of a
multitude of conceptual inventions, including:

* [Dijkstra's
  algorithm](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm) (1959) &ndash;
  provably optimal (given non-negative edges), provably fast (polynomial worst
  case) greedy shortest path algorithm that considers first the nodes closest to
  the start
* [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance)
  (1965) &ndash; the variant of _edit distance_ that considers insertions, deletions
  and subtitutions
* [A* algorithm](https://en.wikipedia.org/wiki/A*_search_algorithm) (1968) &ndash; extension of Dijkstra's algorithm that also considers a heuristic
  estimate of the remaining path length; provably optimal (using an
  _admissibile_/_optimistic_ heuristic), not slower than Dijkstra in the worst
  case (assuming a _consistent_ heuristic), crucial for the result optimality,
  may enormously speed up Dijkstra's algorithm (in average case) when used with
  an informative heuristic
* [Suffix tree](https://en.wikipedia.org/wiki/Suffix_tree) (1973) &ndash; data
  structure used to directly search from all possible starting places of a string

Among the countless other indirect influences, most notable conceptual works include:
* [Alan Turing's early program proof
  work](https://fi.ort.edu.uy/innovaportal/file/20124/1/09-turing_checking_a_large_routine_earlyproof.pdf)
  (1949) 
* [Automata theory](https://en.wikipedia.org/wiki/Automata_theory)
* [Dynamic programming](https://en.wikipedia.org/wiki/Dynamic_programming)
