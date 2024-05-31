#!/bin/bash

THREADS_STEP=4
SIZE_MIN_MAX_WINDOWS=1000
REPETITIONS=10
DEFAULT_N=512
DEFAULT_MIN=0
DEFAULT_MAX=1000
NUM_CORES=40
LOGFILE="wavefront_log.csv"

# parsing command line arguments
# (both optional, first one is the number of cores, second one is the log file)
if [ $# -gt 0 ]; then
    NUM_CORES=$1
    if ! [[ $NUM_CORES =~ ^[0-9]+$ ]] ||
        [ $NUM_CORES -lt 1 ] ||
        [ $NUM_CORES -gt 60 ]; then
        echo "Please provide a number in [1, 60] for the first argument."
        exit 1
    fi
    if [ $# -gt 1 ]; then
        LOGFILE=$2
    fi
fi

######################## EXECUTING SEQUENTIAL VERSION ##########################
echo "Executing sequential version"
for i in $(seq 1 $REPETITIONS); do
    echo "[$ri/$REPETITIONS] -s -l wavefront_seq_log.csv"
    ./wavefront -s -l wavefront_seq_log.csv
done

# empty the log file
truncate -s 0 $LOGFILE

######################## TESTING STRONG SCALABILITY ############################
echo "Testing strong scalability"
for t in $(seq 0 $THREADS_STEP $NUM_CORES); do
    thr=$((t == 0 ? t + 1 : t))
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] T = $thr, N = $DEFAULT_N, min = $DEFAULT_MIN, max = $DEFAULT_MAX"
        ./wavefront -N $DEFAULT_N -T $thr -m $DEFAULT_MIN -M $DEFAULT_MAX -l $LOGFILE
    done
done

######################## TESTING WEAK SCALABILITY ##############################
echo "Testing weak scalability"
for t in $(seq 4 $THREADS_STEP $NUM_CORES); do
    max=$((t*SIZE_MIN_MAX_WINDOWS))
    min=$((max-SIZE_MIN_MAX_WINDOWS))
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] T = $t, N = $DEFAULT_N, min = $min, max = $max"
        ./wavefront -N $DEFAULT_N -T $t -m $min -M $max -l $LOGFILE
    done
done

######################## TESTING WITH UNBALANCED WORKLOADS #####################
echo "Testing with unbalanced workloads"
for max in 100 2000; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] T = $NUM_CORES, N = $DEFAULT_N, min = 0, max = $max"
        ./wavefront -N $DEFAULT_N -T $NUM_CORES -m 0 -M $max -l $LOGFILE
    done
done

######################## TESTING WITH BALANCED WORKLOADS #######################
echo "Testing with balanced workloads"
for m in 0 500; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] T = $NUM_CORES, N = $DEFAULT_N, min = $m, max = $m"
        ./wavefront -N $DEFAULT_N -T $NUM_CORES -m $m -M $m -l $LOGFILE
    done
done