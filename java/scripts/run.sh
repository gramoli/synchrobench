#!/bin/bash

dir=..
output=${dir}/output
deuce="${dir}/lib/mydeuce.jar"
agent=${dir}/lib/deuceAgent-1.3.0.jar
bin=${dir}/bin
java=java
javaopt=-server

### javac options
# -O (dead-code erasure, constants pre-computation...)
### JVM HotSpot options
# -server (Run the JVM in server mode) 
# -Xmx1g -Xms1g (set memory size)
# -Xss2048k (Set stack size)
# -Xoptimize (Use the optimizing JIT compiler) 
# -XX:+UseBoundThreads (Bind user threads to Solaris kernel threads)
###

stms="estm estmmvcc"
syncs="sequential lockbased lockfree transactional"
thread="1 2 4 8 16 32 64"
size="16384 65536"

writes="0 50"
length="5000"
l="5000"
warmup="0"
snapshot="0"
writeall="0"
iterations="5"

JVMARGS=-Dorg.deuce.exclude="java.util.*,java.lang.*,sun.*" 
BOOTARGS=-Xbootclasspath/p:${dir}/lib/rt_instrumented.jar
CP=${dir}/lib/compositional-deucestm-0.1.jar:${dir}/lib/mydeuce.jar:${dir}/bin
MAINCLASS=contention.benchmark.Test

if [ ! -d "${output}" ]; then
  mkdir $output
else
  rm -rf ${output}/*
fi

mkdir ${output}/log ${output}/data ${output}/plot ${output}/ps

###############################
# records all benchmark outputs
###############################

# lockfree benchmarks
benchs="hashtables.lockfree.LFArrayHashSet hashtables.lockfree.NonBlockingCliffHashMap hashtables.lockfree.NonBlockingFriendlyHashMap linkedlists.lockfree.NonBlockingLinkedListSet queues.lockfree.LockFreeQueueIntSet skiplists.lockfree.NonBlockingFriendlySkiplistMap skiplists.lockfree.NonBlockingJavaSkipListMap trees.lockfree.NonBlockingTorontoBSTMap"
if [[ "${syncs}" =~ "lockfree" ]]; then
for bench in ${benchs}; do
  for write in ${writes}; do
   for t in ${thread}; do
    for i in ${size}; do
      r=`echo "2*${i}" | bc`
      out=${output}/log/${bench}-lockfree-i${i}-u${write}-t${t}.log
      for (( j=1; j<=${iterations}; j++ )); do
	  echo "${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -l ${l} -t ${t} -i ${i} -r ${r} -b ${BENCHPATH}.${bench}"
	  ${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -d ${l} -t ${t} -i ${i} -r ${r} -b ${bench} 2>&1 >> ${out}
      done
    done
   done
  done
done
fi


# lock-based benchmarks
benchs="arrays.lockbased.Vector hashtables.lockbased.LockBasedJavaHashMap linkedlists.lockbased.LazyLinkedListSortedSet linkedlists.lockbased.LockedLinkedListIntSet trees.lockbased.LockBasedFriendlyTreeMap trees.lockbased.LockBasedStanfordTreeMap trees.lockbased.LogicalOrderingAVL"
if [[ "${syncs}" =~ "lockbased" ]]; then
for bench in ${benchs}; do
 for write in ${writes}; do
  for t in ${thread}; do
    for i in ${size}; do
	r=`echo "2*${i}" | bc`
	out=${output}/log/${bench}-lockbased-i${i}-u${write}-t${t}.log
	for (( j=1; j<=${iterations}; j++ )); do
	    echo "${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -l ${l} -t ${t} -i ${i} -r ${r} -b ${BENCHPATH}.${bench}"
	    ${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -d ${l} -t ${t} -i ${i} -r ${r} -b ${bench} 2>&1 >> ${out}
	done
    done
  done
 done
done
fi


# sequential benchmark
benchs="arrays.sequential.SequentialVector hashtables.sequential.SequentialHashIntSet linkedlists.sequential.SequentialLinkedListIntSet linkedlists.sequential.SequentialLinkedListSortedSet queues.sequential.SequentialQueueIntSet skiplists.sequential.SequentialSkipListIntSet trees.sequential.SequentialRBTreeIntSet"
if [[ "${syncs}" =~ "sequential" ]]; then
for bench in ${benchs}; do
 for write in ${writes}; do
    for i in ${size}; do
	r=`echo "2*${i}" | bc`
	out=${output}/log/${bench}-sequential-i${i}-u${write}-t1.log
	for (( j=1; j<=${iterations}; j++ )); do
	    echo "${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -l ${l} -t 1 -i ${i} -r ${r} -b ${BENCHPATH}.${bench}"
	    ${java} ${javaopt} -cp ${CP} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -d ${l} -t 1 -i ${i} -r ${r} -b ${bench} 2>&1 >> ${out}
	done
    done
 done
done
fi

# transaction-based benchmarks
benchs="arrays.transactional.Vector hashtables.transactional.TransactionalBasicHashSet linkedlists.transactional.CompositionalLinkedListSortedSet linkedlists.transactional.ElasticLinkedListIntSet linkedlists.transactional.ReusableLinkedListIntSet"
if [[ "${syncs}" =~ "transactional" ]]; then
for bench in ${benchs}; do
 for write in ${writes}; do
  for t in ${thread}; do
    for i in ${size}; do
	  r=`echo "2*${i}" | bc`
      for stm in ${stms}; do
	      out=${output}/log/${bench}-stm${stm}-i${i}-u${write}-t${t}.log
        for (( j=1; j<=${iterations}; j++ )); do
	    echo "${java} ${javaopt} ${JVMARGS} -Dorg.deuce.transaction.contextClass=org.deuce.transaction.${stm}.Context -javaagent:${agent} -cp ${CP} ${BOOTARGS} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -l ${l} -t ${t} -i ${i} -r ${r} -b ${BENCHPATH}.${bench} -v"
	    ${java} ${javaopt} ${JVMARGS} -Dorg.deuce.transaction.contextClass=org.deuce.transaction.${stm}.Context -javaagent:${agent} -cp ${CP} ${BOOTARGS} ${MAINCLASS} -W ${warmup} -u ${write} -a ${writeall} -s ${snapshot} -d ${l} -t ${t} -i ${i} -r ${r} -b ${bench} -v 2>&1 >> ${out}
	done
      done
    done
  done
 done
done
fi

