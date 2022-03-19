#!/bin/bash

compile=$1

if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make freq_elems_accuracy
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

### Which experiments to run?
vsdfsdfu=false #examine impact of df_s and df_u
vsN=false #examine impact of N
vs=true # examine impact of query rate and phi.
vt=true # Vary threads

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["flows_dirA"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
datasets["flows_dirB"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirB.txt"

EPSILONratio="0.1" 

query_rates="0"
topk_rates="0"
threads="24"
K="55555"
MAX_FILTER_SUMS="1000"
MAX_FILTER_UNIQUES="64"
rows=4

reps=2
num_reps=$(seq $reps)

skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
#Calculate number of counters per thread in Delegation Space-Saving
num_counters_deleg (){
    eps=$1
    a=$2
    T=$3
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps * $T))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+2))
    echo $res
}
#Calculate number of counters used by Space-Saving single-threaded
num_counters_single (){
    eps=$1
    a=$2
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+2))
    echo $res
}

num_counters_topkapi(){
    dss_counters_tot=$1
    df_size=$2
    T=$3
    rows=$4
    a=$5
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    aThRoot=$(echo "e( l($dss_counters_tot/$T)/$a )" | bc -l)
    res=$(echo "(16*$aThRoot + $T*2*$df_size + 9*$T) / (2*$rows)" | bc -l)
    res=${res%.*}
    echo $res
}

phi="0.00001"
MAX_FILTER_SUMS="100 1000 10000 100000"
MAX_FILTER_UNIQUES="16 32 64 128"
streamlengths="30000000"
versions="cm_spacesaving_deleg_maxheap_accuracy"
## Vary Skew, df_s and df_u
echo "------ Vary Skew, df_s and df_u------"
if [ "$vsdfsdfu" = true ] ; then 
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            num_thr="24"
            eps=$(echo "$phi*$EPSILONratio" | bc -l)
            eps=0$eps
            calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
            echo "skew: $skew"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for N in $streamlengths
                    do
                        echo "N: $N"
                        for rep in $num_reps
                        do
                            echo "rep: $rep"
                            new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4)))
                            output=$(./bin/"$version".out $N $N $new_columns $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE")
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_${N}_dfsdfu_accuracy.log
                        done
                    done
                done
            done
        done
    done

fi

phi="0.00001"
MAX_FILTER_SUMS="1000"
MAX_FILTER_UNIQUES="64"
streamlengths="1000000 10000000 30000000"
versions="cm_topkapi_accuracy" #"cm_topkapi_accuracy cm_spacesaving_deleg_maxheap_accuracy cm_spacesaving_single_maxheap_accuracy"
## Vary Skew and N
echo "------Vary Skew and N------"
if [ "$vsN" = true ] ; then 
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            if [[ "$version" == *"single"* ]]; then 
                num_thr="1"
                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                eps=0$eps
                calgo_param=$(num_counters_single "$eps" "$skew")
            else
                num_thr="24"
                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                eps=0$eps
                calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
            fi
            dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
            dss_counters=$(( dss_counters*num_thr ))

            K=${topk[${dsname},${skew},${phi}]}
            new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
            echo "skew: $skew"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for N in $streamlengths
                    do
                        echo "N: $N"
                        for rep in $num_reps
                        do
                            echo "rep: $rep"
                            output=$(./bin/"$version".out "$N" "$N" $new_columns $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE)
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_varN_accuracy.log
                        done
                        cp logs/topk_results.txt logs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_varN_histogram.log
                    done
                done
            done
        done
    done
fi

phis="0.001 0.0001 0.00001"
MAX_FILTER_SUM="1000"
MAX_FILTER_UNIQUE="64"
streamlengths="30000000"
versions="cm_spacesaving_deleg_maxheap_accuracy cm_spacesaving_single_maxheap_accuracy cm_topkapi_accuracy"
## Vary Skew and phi (query rate has no effect on accuracy)
echo "------ Vary Skew and phi ------"
if [ "$vs" = true ] ; then 
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            echo "skew: $skew"
            for phi in $phis
            do
                if [[ "$version" == *"single"* ]]; then 
                    num_thr="1"
                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                    eps=0$eps
                    calgo_param=$(num_counters_single "$eps" "$skew")

                else
                    num_thr="24"
                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                    eps=0$eps
                    calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
                fi
                dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
                dss_counters=$(( dss_counters*num_thr ))

                K=${topk[${dsname},${skew},${phi}]}
                new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                for topk_rates in $query_rates
                do
                    for N in $streamlengths
                    do
                        echo "N: $N"
                        for rep in $num_reps
                        do
                            echo "rep: $rep"
                            output=$(./bin/"$version".out $N $N $new_columns $rows 1 $skew 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUE)
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_${N}_phi_accuracy.log
                        done
                    cp logs/topk_space.txt logs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_${N}_memory.log
                    done
                done
            done
        done
    done
fi

phis="0.001 0.0001 0.00001"
MAX_FILTER_SUM="1000"
MAX_FILTER_UNIQUE="64"
streamlengths="30000000"
threads="4 8 12 16 20 24"
skew="1.25" 
versions="cm_spacesaving_deleg_maxheap_accuracy cm_spacesaving_single_maxheap_accuracy cm_topkapi_accuracy"
## Vary threads, and phi
echo "------ Vary Threads and phi------"

if [ "$vt" = true ] ; then 
    for dsname in "${!datasets[@]}"
    do 
        echo "$dsname"
        filepath=${datasets[$dsname]}
        if [[ "$dsname" == " " ]]; then 
            skew="1.25"
            dsname=""
        else
            skew="0.5"
        fi
        for version in $versions
        do 
            if [[ "$version" == *"single"* ]]; then 
                threads="1"
            else
                threads="4 8 12 16 20 24"
            fi
            echo "$version"
            for num_thr in $threads
            do
                echo "skew: $skew"
                for phi in $phis
                do
                    if [[ "$version" == *"single"* ]]; then 
                        eps=$(echo "$phi*$EPSILONratio" | bc -l)
                        eps=0$eps
                        calgo_param=$(num_counters_single "$eps" $skew)
                    else
                        eps=$(echo "$phi*$EPSILONratio" | bc -l)
                        eps=0$eps
                        calgo_param=$(num_counters_deleg "$eps" $skew $num_thr)
                    fi
                    dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
                    dss_counters=$(( dss_counters*num_thr ))

                    K=${topk[${dsname},${skew},${phi}]}
                    new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                    for topk_rates in $query_rates
                    do
                        for N in $streamlengths
                        do
                            echo "N: $N"
                            for rep in $num_reps
                            do
                                echo "rep: $rep"
                                #new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4)))
                                output=$(./bin/"$version".out $N $N $new_columns $rows 1 $skew 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUE $filepath)
                                echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/var_threads_"${version}"_"${num_thr}""_"${skew}_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_${N}_phi"${dsname}"_accuracy.log
                            done
                        done
                    done
                done
            done
        done
    done
fi
