#!/bin/bash

dir=.
output="${dir}/output"

threads="1 4 8 12"
updates_ratios="0 10 100"
sizes="100 1000 10000"

cd ..

benchs="linkedlists.lockbased.CoarseGrainedListBasedSet linkedlists.lockbased.HandOverHandListBasedSet linkedlists.lockbased.LazyLinkedListSortedSet"

for size in ${sizes}; do
  for u in ${updates_ratios}; do
    out="${output}/nowarmup_data/size${size}-update${u}.data"
    touch "${out}"
    chmod a+w "${out}"
    for bench in ${benchs}; do
      for t in ${threads}; do
        r=$(echo "2 * ${size}" | bc)
        input="${output}/nowarmup_log/${bench}-lockbased-i${size}-u${u}-t${t}.log"
        cat "${input}" | grep Throughput | sed -n 's/[^0-9.]\+/ /g;s/^ \+//;s/ \+$//;s/ \+/ /g;p' >> "${out}"
      done
    done
  done
done
