import sys
import os
import re
from statistics import mean, stdev

def read_from_file(filename, keys):
    inf = open(filename, 'r')
    values = dict()
    for key in keys:
        values[key] = []
    for line in inf.readlines():
        ll = line.lower()
        good = None
        for key in keys:
            if key.lower() in ll:
                good = key
        if good == None:
            continue
        value = re.sub('(.*?)', '', ll).strip().split(" ")[-1]
        values[key].append(float(value))
    return values

def filename(bench, size, write, proc, warmup, duration):
   return "../output/log/" + bench + "-i" + str(size) + "-u" + str(write) + "-t" + str(proc) + "-w" + str(warmup) + "-d" + str(duration) + ".log"

keys = ["throughput"]
procs = range(1, 80)#[1, 2, 4, 8, 15, 16, 23, 24, 31, 32, 39, 47, 55, 63, 71, 79]
sizes = [16384, 65536, 262144, 524288, 1048576]
writes = [0, 20, 40, 60, 80, 100]
warmup = 5
duration = 10000
benchmarks = [#"trees.lockbased.ConcurrencyOptimalBSTv2",
              #"trees.lockbased.ConcurrencyOptimalBSTv3",
#              "trees.lockbased.ConcurrencyOptimalBSTv4",
              "trees.lockbased.ConcurrencyOptimalTreeMap",
              "trees.lockbased.ConcurrencyOptimalTreeMapContended",
              "trees.lockbased.LockBasedFriendlyTreeMap",
              "trees.lockbased.LockBasedFriendlyTreeMapNoRotation",
              "trees.lockbased.LogicalOrderingAVL",
              "trees.lockbased.LockBasedStanfordTreeMap",
              "trees.lockfree.NonBlockingTorontoBSTMap"
              ]

error_bars = {
               "trees.lockbased.ConcurrencyOptimalTreeMap",
               "trees.lockbased.LockBasedFriendlyTreeMapNoRotation",
               "trees.lockbased.LockBasedStanfordTreeMap"
             }

directory = "../output/data-w" + str(warmup) + "-d" + str(duration) + "/data_files/"
if not os.path.isdir(directory):
    os.mkdir(directory)

for key in keys:
    for size in sizes:
        for write in writes:
            for i in range(len(benchmarks)):
                bench = benchmarks[i]
                out = open(directory + "trees_comparison_" + str(key) + "_" + str(size) + "_" + str(write) + "_" + str(i) + ".dat", 'w')
                if not os.path.exists(filename(bench, size, write, 1, warmup, duration)):
                    continue
                for proc in procs:
                    if not os.path.exists(filename(bench, size, write, proc, warmup, duration)):
                        continue
                    results = read_from_file(filename(bench, size, write, proc, warmup, duration), keys)[key][1:]
                    out.write(str(proc) + " " + str(mean(results) / 1000 / 1000) + " " + str(stdev(results) / 1000 / 1000) + "\n")

directory = "../output/data-w" + str(warmup) + "-d" + str(duration)
if not os.path.isdir(directory):
    os.mkdir(directory)

path_id = 0
for key in keys:
    for size in sizes:
        out = open(directory + "/trees_comparison_" + str(key) + "_" + str(size) + ".plot", 'w')
        for write in writes:
            out.write("\\begin{tikzpicture}\n")
            out.write("\\begin{axis}[\nlegend style={at={(0.5, -0.1)},anchor=north},\n xlabel={Processors},\n ylabel={Throughput mops/s},\n" +
                      " cycle list name=color,\n title=Update rate: " + str(write) + "\\%\n]\n")
            for i in range(len(benchmarks)):
                bench = benchmarks[i]
                data_file = "data/paristech/data_files/trees_comparison_" + str(key) + "_" + str(size) + "_" + str(write) + "_" + str(i) + ".dat"
                if not os.path.exists(filename(bench, size, write, 1, warmup, duration)):
                    continue
#  Version with error bars
#                if bench in error_bars:
#                    out.write("\\addplot+[error bars/.cd, y dir=both, y explicit] table [x index = 0, y index = 1, y error index = 2] {" + data_file + "};\n")
#                else:
# Version with filled curve
                if True: #bench in error_bars:
                    out.write("\\addplot [name path=pluserror,draw=none,no markers,forget plot] table [x index = 0, y expr=\\thisrowno{1}+\\thisrowno{2}] {" + data_file + "};\n")
                    out.write("\\addplot [name path=minuserror,draw=none,no markers,forget plot] table [x index = 0, y expr=\\thisrowno{1}-\\thisrowno{2}] {" + data_file + "};\n")
                    out.write("\\addplot+[opacity=0.3,forget plot] fill between [of = pluserror and minuserror];\n")
                out.write("\\addplot table [x index = 0, y index = 1] {" + data_file + "};\n")
                out.write("\\addlegendentry{" + bench.split(".")[-1] + "};\n")

            out.write("\\end{axis}\n")
            out.write("\\end{tikzpicture}\n")

for size in sizes:
    out = open(directory + "/trees_comparison_selfspeedup_" + str(size) + ".plot", 'w')
    for write in writes:
        out.write("\\begin{tikzpicture}\n")
        out.write("\\begin{axis}[\nlegend style={at={(0.5, -0.1)},anchor=north},\n xlabel={Processors},\n ylabel={Self-Speedup},\n" +
                  " cycle list name=color,\n smooth,\n title=Update rate: " + str(write) + "\\%\n]\n")
        for i in range(len(benchmarks)):
            bench = benchmarks[i]
            if not os.path.exists(filename(bench, size, write, 1, warmup, duration)):
                continue
            out.write("\\addplot coordinates {\n")
            thr1 = mean(read_from_file(filename(bench, size, write, 1, warmup, duration), keys)[key])
            for proc in procs:
                 if not os.path.exists(filename(bench, size, write, proc, warmup, duration)):
                     continue
                 thr = mean(read_from_file(filename(bench, size, write, proc, warmup, duration), keys)[key])
                 out.write("\t({}, {:.3})\n".format(proc, thr / thr1))
            out.write("};\n")
            out.write("\\addlegendentry{" + bench.split(".")[-1] + "};\n")
        out.write("\\end{axis}\n")
        out.write("\\end{tikzpicture}\n")



out.close()

