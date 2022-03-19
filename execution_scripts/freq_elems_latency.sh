#!/bin/bash
#
# Compare performance of deleg frequent elems vs single threaded frequent elems
#

compile=$1
if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make freq_elems_performance
    cd ../
fi

declare -A topk
topk[flows_dirA,0.5,0.00001]=10435
topk[flows_dirA,0.5,0.0001]=1555
topk[flows_dirA,0.5,0.001]=44
topk[flows_dirB,0.5,0.00001]=15085
topk[flows_dirB,0.5,0.0001]=724
topk[flows_dirB,0.5,0.001]=6
topk["",0.5,0.00001]=54
topk["",0.5,0.0001]=0
topk["",0.5,0.001]=0
topk["",0.75,0.00001]=2884
topk["",0.75,0.0001]=134
topk["",0.75,0.001]=6
topk["",1,0.00001]=6279
topk["",1,0.0001]=629
topk["",1,0.001]=62
topk["",1.25,0.00001]=2952
topk["",1.25,0.0001]=467
topk["",1.25,0.001]=74
topk["",1.5,0.00001]=1135
topk["",1.5,0.0001]=244
topk["",1.5,0.001]=52
topk["",1.75,0.00001]=489
topk["",1.75,0.0001]=131
topk["",1.75,0.001]=35
topk["",2,0.00001]=246
topk["",2,0.0001]=77
topk["",2,0.001]=24
topk["",2.25,0.00001]=140
topk["",2.25,0.0001]=50
topk["",2.25,0.001]=18
topk["",2.5,0.00001]=88
topk["",2.5,0.0001]=35
topk["",2.5,0.001]=14
topk["",2.75,0.00001]=60
topk["",2.75,0.0001]=26
topk["",2.75,0.001]=11
topk["",3,0.00001]=43
topk["",3,0.0001]=20
topk["",3,0.001]=9

### Synthetic stream parameters
universe_size=30000000
stream_size=30000000
num_seconds=4
N=${stream_size}

### Which experiments to run?
vs=false
vt=true

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["flows_dirA"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
datasets["flows_dirB"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirB.txt"

K="55555"
EPSILONratio="0.1"
reps=2
num_reps=$(seq $reps)

num_counters_deleg (){
    eps=$1
    a=$2
    T=$3
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps * $T))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+1))
    echo $res
}

num_counters_single (){
    eps=$1
    a=$2
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+1))
    echo $res
}

buckets=300 #use 800 for odysseus, 512 for ithaca
rows=8

### Investigate effects of df_u and df_s over skew ###
queries="0"
MAX_FILTER_UNIQUES="64"
MAX_FILTER_SUMS="1000"
phis="0.001 0.0001 0.00001"
topkqueriesS="10 100 1000"
versions="cm_spacesaving_single" #"cm_spacesaving_deleg_maxheap cm_spacesaving_single_maxheap cm_spacesaving_deleg"
## Vary skew with qr and phi
echo "------ Vary skew, query rate and phi------"
if [ "$vs" = true ] ; then
    for dsname in "${!datasets[@]}"
    do 
        echo "$dsname"
        filepath=${datasets[$dsname]}
        if [[ "$dsname" == " " ]]; then 
            skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
            dsname=""
        else
            skew_rates="0.5"
        fi
        for version in $versions
        do
            if [[ "$version" == *"single"* ]]; then 
                num_thr="1"
            else
                num_thr="24"
            fi
            echo "$version"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for topkrates in $topkqueriesS
                    do
                        for phi in $phis
                        do
                            for skew in $skew_rates
                            do
                                if [[ "$version" == *"single"* ]]; then 
                                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                    eps=0$eps
                                    calgo_param=$(num_counters_single "$eps" "$skew")
                                else
                                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                    eps=0$eps
                                    calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                fi
                                for _ in $num_reps
                                do
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
                                    output=$(./bin/"$version".out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" "$topkrates" $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath 2>error.txt ) 
                                    echo "$output" | grep -oP 'Average latency: (\d+.\d+)' -a --text >> logs/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_latency.log
                                done
                            done                     
                        done
                    done
                done
            done
        done
    done
fi

MAX_FILTER_UNIQUES="64"
MAX_FILTER_SUMS="1000"
phis="0.001 0.0001 0.00001"
topkqueriesS="10 100 1000"
versions="cm_spacesaving_deleg_maxheap cm_spacesaving_deleg" #"cm_spacesaving_deleg_maxheap cm_spacesaving_single_maxheap cm_spacesaving_deleg"
threads="4 8 12 16 20 24"
## Vary threads with skew 1
echo "------ Vary Threads, query rate and phi------"
if [ "$vt" = true ] ; then
    for dsname in "${!datasets[@]}"
    do 
        echo "$dsname"
        filepath=${datasets[$dsname]}
        if [[ "$dsname" == " " ]]; then 
            skew="1"
            dsname=""
        else
            skew="0.5"
        fi
        for version in $versions
        do
            if [[ "$version" == *"single"* ]]; then 
                thrs="1"
            else
                thrs=$threads
            fi
            echo "$version"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for topkrates in $topkqueriesS
                    do
                        for phi in $phis
                        do
                            for num_thr in $thrs
                            do
                                if [[ "$version" == *"single"* ]]; then 
                                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                    eps=0$eps
                                    calgo_param=$(num_counters_single "$eps" $skew)
                                else
                                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                    eps=0$eps
                                    calgo_param=$(num_counters_deleg "$eps" $skew "$num_thr")
                                fi
                                for _ in $num_reps
                                do
                                    new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    output=$(./bin/"$version".out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 "$num_thr" $queries $num_seconds "$calgo_param" "$topkrates" $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath 2>error.txt ) 
                                    echo "$output" | grep -oP 'Average latency: (\d+.\d+)' -a --text >> logs/threads_"${version}"_"${num_thr}"_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_latency.log
                                    #echo "$output" | grep -oP 'topk percentage \K([0-9]+\.[0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_numqueries_final${filename}.log
                                    #echo "$output" | grep -oP 'num Inserts \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_streamlength_final${filename}.log
                                    #echo "$output" | grep -oP 'num topk \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_numqueriesabs_final${filename}.log
                                done
                            done                     
                        done
                    done
                done
            done
        done
    done
fi
