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
sizes = [16384, 65536]
writes = [0, 20, 40, 60, 80, 100]
warmup = 5
duration = 10000
benchmarks = ["trees.lockbased.ConcurrencyOptimalBSTv2",
              "trees.lockbased.LockBasedFriendlyTreeMap",
              "trees.lockbased.LockBasedStanfordTreeMap",
              "trees.lockfree.NonBlockingTorontoBSTMap"]
directory = "../output/data-w" + str(warmup) + "-d" + str(duration)
if not os.path.isdir(directory):
    os.mkdir(directory)
for write in writes:
    out = open(directory + "/trees_comparison_" + str(write) + ".txt", 'w')
    for key in keys:
        out.write("Key " + key + "\n")
        for size in sizes:
            out.write("Size " + str(size) + "\n")
            buffer = "{:^45}".format("benchmark name")
            for proc in procs:
                buffer += "{:^25}".format(str(proc))
            out.write(buffer + "\n")
            for bench in benchmarks:
                buffer = "{:^45}".format(bench)
                thr1 = mean(read_from_file(filename(bench, size, write, 1, warmup, duration), keys)[key])
                for proc in procs:
                     thr = mean(read_from_file(filename(bench, size, write, proc, warmup, duration), keys)[key])
                     buffer += "{:^25}".format("{0:.3f}".format(thr) + " ({0:.3f})".format(thr / thr1))
                out.write(buffer + "\n")
            out.write("\n")
    out.close()

