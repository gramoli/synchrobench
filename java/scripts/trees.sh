#!/bin/bash

dir=..
output=${dir}/output
deuce="${dir}/lib/mydeuce.jar"
agent=${dir}/lib/deuceAgent-1.3.0.jar
bin=${bin}/bin
java=java
javaopt=-server

threads="1 2 4 8 16 24 32"
sizes="16384 65536"

writes="0 20 40 60 80 100"
duration="10000"
warmup="5"
snapshot="0"
writeall="0"
iterations="5"

JVMARGS=-Dorg.deuce.exclude="java.util.*,java.lang.*,sun.*"
BOOTARGS=-Xbootclasspath/p:${dir}/lib/rt_instrumented.jar
CP=${dir}/lib/compositional-deucestm-0.1.jar:${dir}/lib/mydeuce.jar:${dir}/bin
MAINCLASS=contention.benchmark.Test

if [ ! -d "${output}" ]; then
  mkdir $output
fi
if [ ! -d "${output}/log" ]; then
#  rm -rf ${output}/log
  mkdir $output
fi

#mkdir ${output}/log

benchs="trees.lockbased.ConcurrencyOptimalBSTv2 trees.lockbased.LockBasedFriendlyTreeMap trees.lockbased.LockBasedStanfordTreeMap trees.lockfree.NonBlockingTorontoBSTMap"
for bench in ${benchs}; do
  for write in ${writes}; do
    for t in ${threads}; do
       for i in ${sizes}; do
#         r=`echo  "2*${i}" | bc`
         r=$((2*${i}))
         out=${output}/log/${bench}-i${i}-u${write}-t${t}-w${warmup}-d${duration}.log
         rm ${out}
         for (( j=1; j<=${iterations}; j++ )); do
           echo "${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -d ${duration} -t ${t} -i ${i} -r ${r} -b ${bench}"
           ${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -d ${duration} -t ${t} -i ${i} -r ${r} -b ${bench} 2>&1 >> ${out}
         done
       done
    done
  done
done
