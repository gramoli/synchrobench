threads=( 1 2 3 4 8 16 24 32 64 128 200 )
sizes=( 64 128 256 512 1024 10000 50000 100000 512000 )
updates=( 0 10 20 30 50 80 100 )



for i in "${sizes[@]}"
do
    echo Size: $i
    for j in "${updates[@]}"
    do
        echo Update: $j
        for k in "${threads[@]}" 
        do 
            echo Thread $k
            for (( n = 1; n <= 5; n++ ))
            do
                ./bin/MUTEX-skiplist -A -i $i -u $j -t $k | grep 'txs ' | cut -d "(" -f2 | cut -d "/" -f1 | head -n 1
            done
        done
    done
done

for i in "${sizes[@]}"
do
    echo Size: $i
    for j in "${updates[@]}"
    do
        echo Update: $j
        for k in "${threads[@]}" 
        do 
            echo Thread $k
            for (( n = 1; n <= 5; n++ ))
            do
                ./bin/lazy-numa -A -i $i -u $j -t $k | grep 'txs ' | cut -d "(" -f2 | cut -d "/" -f1 | head -n 1
            done
        done
    done
done

for i in "${sizes[@]}"
do
    echo Size: $i
    for j in "${updates[@]}"
    do
        echo Update: $j
        for k in "${threads[@]}" 
        do 
            echo Thread $k
            for (( n = 1; n <= 5; n++ ))
            do
                ./bin/lockfree-numask-skiplist -A -i $i -u $j -t $k | grep 'txs ' | cut -d "(" -f2 | cut -d "/" -f1 | head -n 1
            done
        done
    done
done
