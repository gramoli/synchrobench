#!/bin/bash

dir=..
output=${dir}/output
deuce="${dir}/lib/mydeuce.jar"
agent=${dir}/lib/deuceAgent-1.3.0.jar

stms="estm estmmvcc"
syncs="sequential lockbased lockfree transactional"
thread="1 8"
size="16384 65536"

writes="100"
length="2000"
l="2000"
warmup="0"
snapshot="0"
writeall="0"
iterations="2"

JVMARGS=-Dorg.deuce.exclude="java.util.*,java.lang.*,sun.*" 
BOOTARGS=-Xbootclasspath/p:${dir}/lib/rt_instrumented.jar
CP=${dir}/lib/compositional-deucestm-0.1.jar:${dir}/lib/mydeuce.jar:${dir}/bin
MAINCLASS=contention.benchmark.Test

###############################
# Extracts values
###############################

benchs="hashtables.lockfree.LFArrayHashSet hashtables.lockfree.NonBlockingCliffHashMap hashtables.lockfree.NonBlockingFriendlyHashMap linkedlists.lockfree.NonBlockingLinkedListSet queues.lockfree.LockFreeQueueIntSet skiplists.lockfree.NonBlockingFriendlySkiplistMap skiplists.lockfree.NonBlockingJavaSkipListMap trees.lockfree.NonBlockingTorontoBSTMap arrays.lockbased.Vector hashtables.lockbased.LockBasedJavaHashMap linkedlists.lockbased.LazyLinkedListSortedSet linkedlists.lockbased.LazyLinkedListSortedSet linkedlists.lockbased.LockBasedLazyListBasedSet skiplists.lockbased.LockedSkipListIntSet trees.lockbased.LockBasedFriendlyTreeMap trees.lockbased.LockBasedStanfordTreeMap trees.lockbased.LogicalOrderingAVL arrays.sequential.SequentialVector hashtables.sequential.SequentialHashIntSet linkedlists.sequential.SequentialLinkedListIntSet linkedlists.sequential.SequentialLinkedListSortedSet queues.sequential.SequentialQueueIntSet skiplists.sequential.SequentialSkipListIntSet trees.sequential.SequentialRBTreeIntSet arrays.transactional.Vector hashtables.transactional.TransactionalBasicHashSet linkedlists.transactional.CompositionalLinkedListSortedSet linkedlists.transactional.ElasticLinkedListIntSet linkedlists.transactional.ReusableLinkedListIntSet"

ds="arrays"
benchs="arrays.lockbased.Vector-lockbased arrays.transactional.Vector-stmestm arrays.sequential.SequentialVector-sequential"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done
# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done


ds="queues"
benchs="queues.lockfree.LockFreeQueueIntSet-lockfree queues.sequential.SequentialQueueIntSet-sequential"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done
# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done


ds="linkedlists"
benchs="linkedlists.lockfree.NonBlockingLinkedListSet-lockfree  linkedlists.lockbased.LazyLinkedListSortedSet-lockbased linkedlists.lockbased.LazyLinkedListSortedSet-lockbased linkedlists.sequential.SequentialLinkedListIntSet-sequential linkedlists.sequential.SequentialLinkedListSortedSet-sequential linkedlists.transactional.CompositionalLinkedListSortedSet-stmestm linkedlists.transactional.ElasticLinkedListIntSet-stmestm linkedlists.transactional.ReusableLinkedListIntSet-stmestmmvcc"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done
# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done


ds="skiplists"
benchs="skiplists.lockfree.NonBlockingFriendlySkiplistMap-lockfree skiplists.lockfree.NonBlockingJavaSkipListMap-lockfree skiplists.sequential.SequentialSkipListIntSet.sequential"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done

# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done


ds="trees"
benchs="trees.lockfree.NonBlockingTorontoBSTMap-lockfree trees.lockbased.LockBasedFriendlyTreeMap-lockfree trees.lockbased.LockBasedStanfordTreeMap-lockfree trees.lockbased.LogicalOrderingAVL-lockbased trees.sequential.SequentialRBTreeIntSet-sequential"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done
# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done


ds="hashtables"
benchs="hashtables.lockfree.LFArrayHashSet-lockfree hashtables.lockfree.NonBlockingCliffHashMap-lockfree hashtables.lockfree.NonBlockingFriendlyHashMap-lockfree  hashtables.lockbased.LockBasedJavaHashMap-lockbased hashtables.transactional.TransactionalBasicHashSet-stmestm hashtables.transactional.TransactionalBasicHashSet-stmestmmvcc hashtables.sequential.SequentialHashIntSet-sequential"
# write header
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
	    printf "#" > ${out}
	    for bench in ${benchs}; do
              printf " ${bench}" >> ${out}
            done
            printf '\n' >> ${out}
	done
    done
# write average
    for write in ${writes}; do
        for i in ${size}; do
            r=`echo "2*${i}" | bc`
            out=${output}/data/${ds}-i${i}-u${write}.log
            for t in ${thread}; do
                printf $t >> ${out}
		for bench in ${benchs}; do 
                    in=${output}/log/${bench}-i${i}-u${write}-t${t}.log
                    thavg=`grep "Throughput" ${in} | awk '{ s += $3; nb++ } END { printf "%f", s/nb }'`
                    upavg=`grep "update" ${in} | awk '{ s += $5; nb++ } END { printf "%f", s/nb }'`
                    #printf " ${thavg} (${upavg})" >> ${out}
                    printf " ${thavg}" >> ${out}
		done

                printf '\n' >> ${out}
            done
        done
    done
