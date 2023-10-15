#!/bin/bash

dir=.
output=${dir}/output

thread="1 4 8 12"
updates_ratio="0 10 100"
size="100 1000 10000"

cd ..
mkdir ${output}/nowarmup_log ${output}/nowarmup_data ${output}/plot ${output}/ps

# lock-based benchmarks
benchs="linkedlists.lockbased.CoarseGrainedListBasedSet linkedlists.lockbased.HandOverHandListBasedSet linkedlists.lockbased.LazyLinkedListSortedSet"
for bench in ${benchs}; do
 for u in ${updates_ratio}; do
  for t in ${thread}; do
    for i in ${size}; do
	r=`echo "2*${i}" | bc`
	out=${output}/nowarmup_log/${bench}-lockbased-i${i}-u${u}-t${t}.log
	echo "java -cp bin contention.benchmark.Test -b ${benchs} -d 2000 -u ${u} -t ${t} -i ${i} -r ${r}"
	    java -cp bin contention.benchmark.Test -b ${benchs} -d 2000 -u ${u} -t ${t} -i ${i} -r ${r} 2>&1 >> ${out}
    done
  done
 done
done
