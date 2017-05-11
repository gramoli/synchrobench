#!/bin/bash

###
# This script run synchrobench c-cpp with different data structures
# and synchronization techniques as 'benchs' executables, with thread
# counts 'threads', inital structure sizes 'sizes', update ratio 'updates'
# sequential benchmarks 'seqbenchs', dequeue benchmarks 'deqbenchs' and 
# outputs the throughput in separate log files 
# '../log/${bench}-n${thread}-i${size}-u${upd}.${iter}.log'
#
# Select appropriate parameters below
#
#threads="1 4 8 12 16 20 24 28 32"
threads="4"
#benchs="tcmalloc-lockfree-ll tcmalloc-spinlock-ht tcmalloc-estm-rt tcmalloc-estm-sl tcmalloc-fraser-sl tcmalloc-rotating-sl tcmalloc-spinlock-ll tcmalloc-estm-ll tcmalloc-estm-st tcmalloc-lockfree-ht tcmalloc-spinlock-sl tcmalloc-estm-ht tcmalloc-spinlock-btree"
benchs="ESTM-hashtable lockfree-hashtable MUTEX-hashtable"
#lockfree-fraser-skiplist lockfree-rotating-skiplist lockfree-nohotspot-skiplist SPIN-skiplist"
#seqbenchs="tcmalloc-sequential-ll tcmalloc-sequential-rt tcmalloc-sequential-ht tcmalloc-sequential-sl"
seqbenchs="sequential-hashtable"
#iterations="1 2 3 4 5 6 7 8 9 10"
iterations="1 2"
#updates="0 100"
updates="10"
#size="1024 4096 8192 16384 32768 65536"
sizes="1024"
#deqbenchs="estm-dq tcmalloc-estm-dq tcmalloc-sequential-dq"
###

# set a memory allocator here
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
echo "LD_PATH:" $LD_LIBRARY_PATH

# path to binaries
bin=../bin

if [ ! -d "../log" ]; then
	mkdir ../log	
fi

for size in ${sizes}
do
# make the range twice as large as initial size to maintain size expectation 
r=`echo "2*${size}" | bc`
for upd in ${updates}
do 
 for thread in ${threads}
 do
  for iter in ${iterations}
  do
   for bench in ${benchs}
   do 
     ${bin}/${bench} -u ${upd} -i ${size} -r ${r} -d 5000 -t ${thread} -f 0 > ../log/${bench}-n${thread}-i${size}-u${upd}.${iter}.log
   done
   echo "Done experimenting concurrent benchs for 5000 milliseconds each"
  done
 done
done
done

# for sequential
for size in ${sizes}
do
r=`echo "2*${size}" | bc`
 for upd in ${updates}
 do 
  for iter in ${iterations}
  do
   for bench in ${seqbenchs}
   do 
     ${bin}/${bench} -u ${upd} -i ${size} -r ${r} -d 5000 -t 1 -f 0 > ../log/${bench}-i${size}-u${upd}-sequential.${iter}.log
   done
  done
 done
done

# for dequeue
for upd in ${updates}
do 
 for thread in ${threads}
 do
  for iter in ${iterations}
  do
   for bench in ${deqbenchs}
   do 
     ${bin}/${bench} -i 128 -r 256 -d 5000 -t ${thread} > ../log/${bench}-n${thread}.${iter}.log
   done
  done
 done
done

# for the lock-coupling linked list
#benchs="tcmalloc-spinlock-ll tcmalloc-spinlock-ht"
benchs=""
for size in ${sizes}
do
 r=`echo "2*${size}" | bc`
 for upd in ${updates}
 do 
  for thread in ${threads}
  do
   for iter in ${iterations}
   do
    for bench in ${benchs}
    do 
     ${bin}/${bench} -x2 -u ${upd} -i ${size} -r ${r} -d 5000 -t ${thread} -f 0 > ../log/${bench}-n${thread}-i${size}-u${upd}-lazy.${iter}.log
    done
   done
  done
 done
done

