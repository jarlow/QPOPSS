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

#Topk for each dataset
declare -A topk
regex="SKEW:[[:space:]]*([0-9]\.{0,1}[1-9]{0,2})[[:space:]]*NUM TOPK:[[:space:]]*([0-9]+)[[:space:]]*PHI:[[:space:]]*(0\.[0-9]+)[[:space:]]*FILEPATH:[[:space:]]*.*\/datasets\/(.*)\.txt"

### Synthetic stream parameters
universe_size=10000000
stream_size=10000000
num_seconds=10
N=${stream_size}
rows=4
new_columns=100
queries=0

### Which experiments to run?
vsdfsdfu=true
vs=true
vt=true

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["flows_dirA"]="/home/victor/git/DelegationSpace-Saving/datasets/flows_dirA.txt"
datasets["flows_dirB"]="/home/victor/git/DelegationSpace-Saving/datasets/flows_dirB.txt"

K="55555"
EPSILONratio="0.1"
reps=2
num_reps=$(seq $reps)
beta=0.01

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


### Investigate effects of df_u and df_s over skew ###
queries="0"
MAX_FILTER_SUMS="100 1000 10000 100000"
MAX_FILTER_UNIQUES="16 32 64 128"
skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
phis="0.0001"
topkqueriesS="100"
versions="cm_spacesaving_deleg_min_max_heap_throughput"
echo "------ Vary Skew, df_s and df_u ------"
if [ "$vsdfsdfu" = true ] ; then
    mkdir -p logs/throughput/vsdfsdfu
    for version in $versions
    do
        if [[ "$version" == *"single"* ]]; then 
            num_thr="1"
        else
            num_thr="24"
        fi
        echo $version
        for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
        do
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for topkrates in $topkqueriesS
                do
                    for phi in $phis
                    do
                        for skew in $skew_rates
                        do
                            for _ in $num_reps
                            do
                                filename="/home/victor/git/DelegationSpace-Saving/datasets/zipf_${skew}_${N}.txt"
                                echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE $beta $filename"
                                output=$(./bin/$version.out $universe_size $stream_size $new_columns $rows 1 "$skew" 0 1 $num_thr $queries $num_seconds "$calgo_param" $topkrates $K $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE" $beta $filename) 
                                echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/throughput/vsdfsdfu/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_dfsdfu_throughput.log
                            done
                        done                     
                    done
                done
            done
        done
    done
fi
MAX_FILTER_UNIQUES="16"
MAX_FILTER_SUMS="1000"
phis="0.001 0.0002 0.0001"
#topkqueriesS="0 100 1000"
topkqueriesS="100"
versions="cm_spacesaving_deleg_min_max_heap_throughput cm_topkapi_throughput cm_spacesaving_single_min_max_heap_throughput cm_spacesaving_deleg_min_heap_throughput"
## Vary skew with qr and phi
echo "------ Vary skew, query rate and phi------"
if [ "$vs" = true ] ; then
    mkdir -p logs/throughput/vs
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
                                if [[ "$dsname" == "" ]]; then
                                    filepath="/home/victor/git/DelegationSpace-Saving/datasets/zipf_${skew}_${stream_size}.txt"
                                fi
                                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                eps=0$eps
                                if [[ "$version" == *"single"* ]]; then 
                                    calgo_param=$(num_counters_single "$eps" "$skew")
                                else
                                    calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                fi
                                dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                dss_counters=$(( dss_counters*num_thr ))
                                
                                if [[ "$version" == *"topkapi"* ]]; then
                                    K=${topk[${skew}${phi}${dsname}]}
                                else
                                    K=999
                                fi
                                
                                new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                                for _ in $num_reps
                                do
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    output=$(./bin/"$version".out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" "$topkrates" $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $beta $filepath) 
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/throughput/vs/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
                                    echo "$output"
                                    if [[ "$version" == *"deleg"* ]]; then
                                        if [[ "$output" =~ $regex ]]; then
                                            if [[ "${BASH_REMATCH[4]}" != *"flows"* ]]; then 
                                                BASH_REMATCH[4]=""
                                            fi
                                            topk[${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}]=${BASH_REMATCH[2]}
                                        else
                                            echo "dosnt match :/ "
                                        fi
                                    fi
                                done
                            done                     
                        done
                    done
                done
            done
        done
    done
fi

MAX_FILTER_UNIQUES="16"
MAX_FILTER_SUMS="1000"

phis="0.001 0.0002 0.0001"
#topkqueriesS="0 100 1000"
topkqueriesS="100"
versions="cm_spacesaving_deleg_min_max_heap_throughput cm_topkapi_throughput cm_spacesaving_single_min_max_heap_throughput cm_spacesaving_deleg_min_heap_throughput"
threads="4 8 12 16 20 24"
## Vary threads with skew 1.25
echo "------ Vary Threads, query rate and phi------"
if [ "$vt" = true ] ; then
    mkdir -p logs/throughput/vt
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
                                if [[ "$dsname" == "" ]]; then
                                    filepath="/home/victor/git/DelegationSpace-Saving/datasets/zipf_${skew}_${stream_size}.txt"
                                fi
                                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                eps=0$eps
                                if [[ "$version" == *"single"* ]]; then 
                                    calgo_param=$(num_counters_single "$eps" "$skew")
                                else
                                    calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                fi

                                dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                dss_counters=$(( dss_counters*num_thr ))

                                if [[ "$version" == *"topkapi"* ]]; then
                                    K=${topk[${skew}${phi}${dsname}]}
                                else
                                    K=999
                                fi
                                new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                                for _ in $num_reps
                                do
                                    #new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    output=$(./bin/"$version".out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 "$num_thr" $queries $num_seconds "$calgo_param" "$topkrates" $K "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $beta $filepath) 
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/throughput/vt/threads_"${version}"_"${num_thr}"_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
                                    echo "$output" 
                                    if [[ "$version" == *"deleg"* ]]; then
                                        if [[ "$output" =~ $regex ]]; then
                                            if [[ "${BASH_REMATCH[4]}" != *"flows"* ]]; then 
                                                BASH_REMATCH[4]=""
                                            fi
                                            topk[${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}]=${BASH_REMATCH[2]}
                                        else
                                            echo "dosnt match :/ "
                                        fi
                                    fi
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
