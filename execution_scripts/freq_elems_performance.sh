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

### Synthetic stream parameters
universe_size=30000000
stream_size=30000000
num_seconds=4
N=${stream_size}

### Which experiments to run?
vsdfsdfu=true
vs=true
vt=true

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["caida_dst_ip"]="/home/victor/git/DelegationSpace-Saving/caida_dst_ip.txt"
datasets["caida_dst_port"]="/home/victor/git/DelegationSpace-Saving/caida_dst_port.txt"

K="55555"
EPSILONratio="0.1"
reps=10
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
MAX_FILTER_SUMS="100 1000 10000 100000"
MAX_FILTER_UNIQUES="16 32 64 128"
skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
phis="0.00001"
topkqueriesS="100"
versions="cm_spacesaving_deleg"
echo "------ Vary Skew, df_s and df_u ------"
if [ "$vsdfsdfu" = true ] ; then
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
                                echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE $filepath"
                                new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
                                #./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE 2>error.txt
                                output=$(./bin/$version.out $universe_size $stream_size $new_columns $rows 1 "$skew" 0 1 $num_thr $queries $num_seconds "$calgo_param" $topkrates $K $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE" 2>error.txt ) 
                                echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_dfsdfu_throughput.log
                                #echo "$output" | grep -oP 'num Inserts \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_"${MAX_FILTER_SUM}"_dfsum"${MAX_FILTER_UNIQUE}"_dfsum_streamlength_finaldfu_dfs.log
                                #echo "$output" | grep -oP 'num topk \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_"${MAX_FILTER_SUM}"_dfsum"${MAX_FILTER_UNIQUE}"_dfsum_numqueriesabs_finaldfu_dfs.log
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
topkqueriesS="0 10 100 1000"
versions="cm_spacesaving_deleg cm_spacesaving_single"
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
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
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


MAX_FILTER_UNIQUES="64"
MAX_FILTER_SUMS="1000"
phis="0.001 0.0001 0.00001"
topkqueriesS="0 10 100 1000"
versions="cm_spacesaving_deleg cm_spacesaving_single"
threads="4 8 12 16 20 24"
## Vary threads with skew 1.25
echo "------ Vary Threads, query rate and phi------"
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
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> logs/threads_"${version}"_"${num_thr}"_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
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
