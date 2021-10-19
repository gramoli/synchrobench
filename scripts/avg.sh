#!/bin/bash

### 
# This script computes the average throughput
# taken from different the log files 
# '../log/${bench}-n${thread}-i${s}-u${upd}.${iter}.log'
# with benchmark 'bench', number of threads 'thread'
# the size 's', the update ratio 'upd' and the iteration
# index 'iter' into a single file:
# '../data/i[size]-synchrobench-test.log'.
#
# Set the parameters of your choice below:
#
#size="1024 4096 8192 16384 32768 65536"
size="1024"
#threads="1 4 8 12 16 20 24 28 32"
threads="4"
#updates="0 100"
update="10"
#benchs="tcmalloc-lockfree-ll tcmalloc-spinlock-ht tcmalloc-estm-rt tcmalloc-estm-sl tcmalloc-fraser-sl tcmalloc-rotating-sl tcmalloc-spinlock-ll tcmalloc-estm-ll tcmalloc-estm-st tcmalloc-lockfree-ht tcmalloc-spinlock-sl tcmalloc-estm-ht tcmalloc-spinlock-btree"
benchs="ESTM-hashtable lockfree-hashtable MUTEX-hashtable"
#iterations="1 2 3 4 5 6 7 8 9 10"
iterations="1 2"
suffix="synchrobench-test"
###

if [ ! -d "../data" ]; then
        mkdir ../data
fi

for s in ${size}
do
 file=../data/i${s}-${suffix}.log

printf "# " > $file
for bench in ${benchs}
do 
 printf "${bench} " >> $file
done
printf "\n" >> ${file}

for upd in ${updates}
do
for thread in ${threads}
do
 printf "${thread} " >> ${file}
 for bench in ${benchs}
 do 
  cpt=0
  total=0
  for iter in ${iterations}
  do
    # sequential
    #thgt=`awk '/#txs/ {print $4}' ../log/${bench}-i${s}-u${upd}-sequential.${iter}.log | cut -c2-`
    # dequeue
    #thgt=`awk '/#txs/ {print $4}' ../log/${bench}-n${thread}.${iter}.log | cut -c2-`
    # lazy
    #thgt=`awk '/#txs/ {print $4}' ../log/${bench}-n${thread}-i${s}-u${upd}-lazy.${iter}.log | cut -c2-`
    # others
    thgt=`awk '/#txs/ {print $4}' ../log/${bench}-n${thread}-i${s}-u${upd}.${iter}.log | cut -c2-`
    total=`echo "scale=3; ${thgt} + ${total}" | bc`
    cpt=`echo "${cpt} + 1" | bc`
  done
  avg=`echo "scale=3; ${total}/${cpt}" | bc`
  printf "${avg} " >> ${file}
 done
 printf "\n" >> ${file}
done
done
printf "\n" >> ${file}
done
