import sys
import os
import re
from statistics import mean

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
procs = [1, 2, 4, 8, 16, 24, 32, 39, 47, 55, 63, 71, 79]
sizes = [16384, 65536, 262144, 524288, 1048576]
writes = [0, 20, 40, 60, 80, 100]
warmup = 5
duration = 10000
benchmarks = [#"trees.lockbased.ConcurrencyOptimalBSTv2",
              #"trees.lockbased.ConcurrencyOptimalBSTv3",
#              "trees.lockbased.ConcurrencyOptimalBSTv4",
              "trees.lockbased.ConcurrencyOptimalTreeMap",
              "trees.lockbased.LockBasedFriendlyTreeMap",
#              "trees.lockbased.LockBasedFriendlyTreeMapNoRotation",
              "trees.lockbased.LogicalOrderingAVL",
#              "trees.lockbased.LockBasedStanfordTreeMap",
#              "trees.lockfree.NonBlockingTorontoBSTMap"
              ]

directory = "../output/data-w" + str(warmup) + "-d" + str(duration)
if not os.path.isdir(directory):
    os.mkdir(directory)
for key in keys:
    for size in sizes:
        out = open(directory + "/trees_comparison_" + str(key) + "_" + str(size) + ".plot", 'w')
        for write in writes:
            out.write("\\begin{tikzpicture}\n")
            out.write("\\begin{axis}[\nlegend style={at={(0.5, -0.1)},anchor=north},\n xlabel={Processors},\n ylabel={Throughput mops/s},\n" +
                      " cycle list name=color,\n title=Update rate: " + str(write) + "\\%\n]\n")
            for i in range(len(benchmarks)):
                bench = benchmarks[i]
                if not os.path.exists(filename(bench, size, write, 1, warmup, duration)):
                    continue
                thr1 = mean(read_from_file(filename(bench, size, write, 1, warmup, duration), keys)[key])
                out.write("\\addplot coordinates {\n")
                for proc in procs:
                     if not os.path.exists(filename(bench, size, write, proc, warmup, duration)):
                         continue
                     thr = mean(read_from_file(filename(bench, size, write, proc, warmup, duration), keys)[key])
                     out.write("\t({}, {:.3})\n".format(proc, int(thr / 1000) / 1000))
                out.write("};\n")
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

