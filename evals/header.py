import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
from IPython.display import display
from pathlib import Path
import seaborn as sns
import math
#%matplotlib inline

pd.set_option('display.max_rows', 500)
pd.set_option('display.max_columns', 500)
pd.set_option('display.width', 1000)

def read_benchmarks_aggregation(benchmarks_file):
    df = pd.read_csv(benchmarks_file, sep='\t', index_col=False)
    #df['algo'] = pd.Categorical(df['algo'], ["graphaligner", "dijkstra", "astar-prefix", "astar-seeds", "pasgal"])
    #df['c'] = df['algo'].apply(algo2color)
    #df['marker'] = df['m'].apply(readlen2marker)
    df['head_Mbp'] = df['head'] / 10**6
    return df

def num_lower(serie):
    return serie.apply(lambda s: sum(1 for c in s if c.islower()))

def read_astarix_performance(tsv_fn, algo=None):
    df = pd.read_csv(tsv_fn, delim_whitespace=True)
    df['pushed+popped'] = df['pushed'] + df['popped']
    df['explored_per_bp'] = df['explored_states'] / df['len']
    df['t(map)_per_bp'] = df['t(map)'] / df['len']
    if 'crumbs' in df:
        df['crumbs_per_bp'] = df['crumbs'] / df['len']
    df['error_rate'] = df['cost'] / df['len']
    #df['generated_errors'] = df['readname'].apply(lambda rn: int(rn.split()[0]) if rn.split()[0].isdigit() else -1)  # TODO: uncomment
    #df['explored_states'] = df['pushed'] * df['len']
    #df['algo'] = df['algo'].replace(['astar-prefix'], 'astarix')
    if algo:
        df['algo'] = algo
    else:
        df['algo'] = pd.Categorical(df['algo'], ["graphaligner", "dijkstra", "astar-prefix", "astar-seeds", "pasgal"], ordered=True)
    #df['algo'] = df['algo'].cat.remove_unused_categories()
    #df['performance'] = df['len'] / df['t(map)'] / 1000000  # [MBp/sec]
    #if 'spell' in df:  # TODO: uncomment
    #    df['dist'] = num_lower(df['spell'])
    #return df.set_index('readname', verify_integrity=True)
    return df.set_index('readname', verify_integrity=False)

def algo2color(algo):
    d = {
        'astarix': 'red', #'mediumseagreen', #' forestgreen',
        'astarix-prefix': 'red',
        'astar-prefix': 'red',
        'astarix-seeds': 'mediumseagreen',
        'astar-seeds': 'mediumseagreen',
        'astarix-seeds_wo_skip_near_crumbs_pos': 'yellow',  ## ablation
        'astarix-seeds_wo_match_pos': 'orange',   ## ablation
        'dijkstra': 'darkorange',
        'graphaligner': 'green',
        'pasgal': 'cornflowerblue',
        'astar-seeds-intervals': 'red',
        'astarix-seeds-intervals': 'red',
        'vargas': 'blue',
        'vg': 'orange',
        }
    if algo in d:
        return d[algo]
    print(algo)
    assert(False)

def algo2beautiful(algo):
    d = {
        'astar': 'A*',
        'astarix': 'AStarix',
        'astarix-seeds': 'Seeds heuristic',
        'astar-seeds': 'Seeds heuristic',
        'astarix-seeds-intervals': 'Seeds heuristic (+intervals)',
        'astar-seeds-intervals': 'Seeds heuristic (+intervals)',
        'astarix-seeds_wo_skip_near_crumbs_pos': 'Seeds -near crumbs',  ## ablation
        'astarix-seeds_wo_match_pos': 'Seeds -match positions',   ## ablation
        'astarix-prefix': 'Prefix heuristic',
        'astar-prefix': 'Prefix heuristic',
        'dijkstra': 'Dijkstra',
        'graphaligner': 'GraphAligner',
        'pasgal': 'PaSGAL',
        'vargas': 'Vargas',
        'vg': 'VG',
        }
    if algo in d:
        return d[algo]
    print(algo)
    assert(False)
    
def col2name(col):
    d = {
        'head':    'Reference length',
        'head_Mbp':'Reference length',
        's':       'Runtime',
        'N':       'Reads',
        'm':       'Read length',
        'max_rss': 'Memory',
        'score':   'Alignment cost',
        'explored_states':  'Explored states',
        't(map)':  'Alignment time per read',  #  [sec/read]
        't(map)_per_bp': 'Alignment time per bp',
        'align_sec':  'Alignment time',
        'cost':    'Alignment cost',
        'explored_per_bp': 'Explored states per bp',
        'error_rate': 'Error rate',
        #'performance': 'MBp/sec'
        }
    if col in d:
        return d[col]
    print(col)
    return col

def col2unit(col):
    d = {
        'head':    'bp',
        'head_Mbp':'Mbp',
        's':       's',
        'N':       '',
        'm':       'bp',
        'max_rss': 'MB',
        }
    if col in d:
        return d[col]
    print(col)
    return col

def readlen2style(readlen):
    d = {
        50:    '.',
        75:    ':',
        100:   '-o',
        150:   '-o',
        }
    if readlen in d:
        return d[readlen]
    print(readlen)
    assert(false)

def readlen2marker(readlen):
    # 'o', 'v', '^', '<', '>', '8', 's', 'p', '*', 'h', 'H', 'D', 'd', 'P', 'X'
    d = {
        75:    '^',
        100:   'o',
        150:   's',
        }
    if readlen in d:
        return d[readlen]
    print(readlen)
    assert(false)
    
def eq(a, b):
    return abs(a-b) < 1e-4

def myticks(num, pos):
        if num == 0: return "$0$"
        exponent = int(np.log10(num))
        coeff = num/10**exponent
        if eq(coeff, 1.0):
            #return r"{:2.0f}".format(num)
            if eq(exponent, 1.0):
                return r"$10$"
            return r"$10^{{ {:2d} }}$".format(exponent)
        assert(False)
        return r"${:2.0f} \times 10^{{ {:2d} }}$".format(coeff,exponent)
