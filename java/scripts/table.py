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
procs = [1, 2, 4, 8, 16, 24, 32]
sizes = [16384, 65536, 262144, 524288, 1048576]
writes = [0, 20, 40, 60, 80, 100]
warmup = 5
duration = 10000
benchmarks = [#"trees.lockbased.ConcurrencyOptimalBSTv2",
              #"trees.lockbased.ConcurrencyOptimalBSTv3",
              "trees.lockbased.ConcurrencyOptimalBSTv4",
              "trees.lockbased.ConcurrencyOptimalTreeMap",
              "trees.lockbased.LockBasedFriendlyTreeMap",
              "trees.lockbased.LockBasedFriendlyTreeMapNoRotation",
              "trees.lockbased.LogicalOrderingAVL",
              "trees.lockbased.LockBasedStanfordTreeMap",
              "trees.lockfree.NonBlockingTorontoBSTMap"]
directory = "../output/data-w" + str(warmup) + "-d" + str(duration)
if not os.path.isdir(directory):
    os.mkdir(directory)
for key in keys:
    for size in sizes:
        out = open(directory + "/trees_comparison_" + str(key) + "_" + str(size) + ".table", 'w')
        out.write("\\begin{tabular}{|c|c|c|c|c|c|c|c|c|}\n")
        out.write("\\hline\n")
        buffer = "writes & {:^45}".format("benchmark name")
        for proc in procs:
            buffer += "& {:^25}".format(str(proc))
        out.write(buffer + "\\\\\\hline\n")
        for write in writes:
            out.write("\\multirow{" + str(len(benchmarks)) + "}{*}{" + str(write) + "\%}")
            for i in range(len(benchmarks)):
                bench = benchmarks[i]
                buffer = "& {:^45}".format(bench.split(".")[-1])
                thr1 = mean(read_from_file(filename(bench, size, write, 1, warmup, duration), keys)[key])
                for proc in procs:
                     thr = mean(read_from_file(filename(bench, size, write, proc, warmup, duration), keys)[key])
                     buffer += "& {:^25}".format("{0:d}k".format(int(thr / 100)) + " ({0:.2f})".format(thr / thr1))
                out.write(buffer + ("\\\\\\hline\n" if i == len(benchmarks) - 1 else "\\\\\\cline{2-" + str(len(procs) + 2) + "}\n"))
        out.write("\\end{tabular}\n")
out.close()

