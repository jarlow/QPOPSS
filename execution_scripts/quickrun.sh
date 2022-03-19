#!/bin/bash
compile=$1
if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make freq_elems_performance
    cd ../
fi

#Topk for each dataset
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
num_thr='24'

rows=4

universe_size="10000000"
stream_size="10000000"
skew="1"
num_seconds=4
EPSILONratio="0.1"


#Real Data
#filename="/home/victor/git/Delegation-Space-Saving/words.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_ip.txt"
#filename="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_port.txt"
#Synthetic data 
filename="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${stream_size}.txt"
topk_rates="1000"
queries="0"
phi="0.001"
MAX_FILTER_SUM="1000"
K=1000
MAX_FILTER_UNIQUES="16"
regex="SKEW:[[:space:]]*([0-9]\.{0,1}[1-9]{0,2})[[:space:]]*NUM TOPK:[[:space:]]*([0-9]+)[[:space:]]*PHI:[[:space:]]*(0\.[0-9]+)[[:space:]]*FILEPATH:[[:space:]]*.*\/datasets\/(.*)\.txt"
versions="cm_spacesaving_deleg_maxheap" #"cm_spacesaving_deleg cm_spacesaving_deleg_maxheap cm_topkapi" #cm_topkapi_accuracy #cm_spacesaving_deleg_accuracy cm_spacesaving_deleg_maxheap_accuracy
#while true==true;do
    for version in $versions
    do
        eps=$(echo "$phi*$EPSILONratio" | bc -l)
        eps=0$eps
        dss_counters=$(num_counters_deleg "$eps" "$skew" "$num_thr")
        dss_counters=$(( dss_counters*num_thr ))

        #K=${topk[${dsname},${skew},${phi}]}
        new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
        echo "K ${K}"
        calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
        #new_columns=$((calgo_param*num_thr)) #$(echo "scale=0; 1/($eps)" | bc -l)
        echo "buckets per thread: ${new_columns}"
        echo "counters per thread: ${calgo_param}"
        echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename"
        output=$(./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename)
        echo "$output"
        if [[ "$output" =~ $regex ]]; then
            if [[ "${BASH_REMATCH[4]}" != *"flows"* ]]; then 
                BASH_REMATCH[4]=""
            fi
            KEY="${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}"
            topk[${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}]=${BASH_REMATCH[2]}
            echo "skew ${BASH_REMATCH[1]} phi ${BASH_REMATCH[3]} dataset ${BASH_REMATCH[4]}"
            echo "${topk[${KEY}]}"
            ds=""
            #KEYIN="${skew}${phi}${ds}"
            echo "${topk[${skew}${phi}${ds}]}"
        else
            echo "dosnt match :/ "
        fi
    done
#done
#echo ""
eps=$(echo "$phi*$EPSILONratio" | bc -l)
eps=0$eps
#calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
#calgo_param=$((calgo_param*num_thr))
calgo_param=$(num_counters_single "$eps" "$skew")
num_thr="1"
#eps="0.0000119" 
echo "spacesaving single"
echo "counters: ${calgo_param}"
new_columns="100"
#K=${topk[${dsname},${skew},${phi}]}
K=1000
echo "$K"
echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename"
./bin/cm_spacesaving_single_maxheap.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename
#./bin/cm_spacesaving_single.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename
