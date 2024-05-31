#!/bin/bash

TOPK=5
REPETITIONS=10
NUM_CORES=60
LOGFILE="./results/word_count_log.csv"
ERRORFILE="./results/error_log.txt"
DIFFFILE="./results/diff_log.txt"

mkdir ./results/

# parsing number of cores from command line arguments
if [ $# -gt 0 ]; then
    NUM_CORES=$1
    if [ $NUM_CORES -ne 40 ] && [ $NUM_CORES -ne 60 ]; then
        echo "Number of cores must be 40 or 60."
        exit 1
    fi
fi

if [ "$NUM_CORES" -eq 40 ]; then
    thread_seq="1 2 4 8 12 16 20 24 28 32 36 40"
else
    thread_seq="1 2 4 8 12 20 28 36 44 52 60"
fi

######################## EXECUTING SEQUENTIAL VERSION ##########################

# empty the log file for time measurements
truncate -s 0 $LOGFILE

echo "Executing sequential version"
for w in 0 1000 10000; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] Word-Count-seq /opt/SPMcode/A2/filelist.txt $w $TOPK 1"
        ./Word-Count-seq /opt/SPMcode/A2/filelist.txt $w $TOPK 1 > "./results/seq_output.txt"
    done
done

# rename the log file
mv $LOGFILE "./results/word_count_log_seq.csv"

####################### EXECUTING PARALLEL VERSIONS ############################

# PARALLEL IMPLEMENTATION WITH A MAP PER THREAD

# empty the log file for time measurements
truncate -s 0 $LOGFILE
# empty the log file for errors
truncate -s 0 $ERRORFILE
# empty the log file for output differences
truncate -s 0 $DIFFFILE

echo "Executing parallel version"
for t in $thread_seq; do
    for w in 0 1000 10000; do
        for rep in $(seq 1 $REPETITIONS); do
            echo "[$rep/$REPETITIONS] Word-Count-maps /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1"
            ./Word-Count-maps /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1 > "./results/par_output.txt"
            DIFF=$(diff "./results/seq_output.txt" "./results/par_output.txt")
            if [ "$DIFF" != "" ]; then
                echo "--------" >> $DIFFFILE
                echo Word-Count-maps /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1 >> $DIFFFILE
                echo $DIFF >> $DIFFFILE
                echo "--------" >> $DIFFFILE
                echo /opt/SPMcode/A2/filelist.txt,$t,$w,$TOPK,NOK >> $ERRORFILE
            else
                echo /opt/SPMcode/A2/filelist.txt,$t,$w,$TOPK,OK >> $ERRORFILE
            fi
        done
    done
done

# rename log files
mv $LOGFILE "./results/word_count_log_maps.csv"
mv $ERRORFILE "./results/error_log_maps.csv"
mv $DIFFFILE "./results/diff_log_maps.txt"

# PARALLEL IMPLEMENTATION WITH A SINGLE MAP

# empty the log file for time measurements
truncate -s 0 $LOGFILE
# empty the log file for errors
truncate -s 0 $ERRORFILE
# empty the log file for output differences
truncate -s 0 $DIFFFILE

echo "Executing parallel version"
for t in $thread_seq; do
    for w in 0 1000 10000; do
        for rep in $(seq 1 $REPETITIONS); do
            echo "[$rep/$REPETITIONS] Word-Count-critical /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1"
            ./Word-Count-critical /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1 > "./results/par_output.txt"
            DIFF=$(diff "./results/seq_output.txt" "./results/par_output.txt")
            if [ "$DIFF" != "" ]; then
                echo "--------" >> $DIFFFILE
                echo Word-Count-critical /opt/SPMcode/A2/filelist.txt $t $w $TOPK 1 >> $DIFFFILE
                echo $DIFF >> $DIFFFILE
                echo "--------" >> $DIFFFILE
                echo /opt/SPMcode/A2/filelist.txt,$t,$w,$TOPK,NOK >> $ERRORFILE
            else
                echo /opt/SPMcode/A2/filelist.txt,$t,$w,$TOPK,OK >> $ERRORFILE
            fi
        done
    done
done

# rename log files
mv $LOGFILE "./results/word_count_log_critical.csv"
mv $ERRORFILE "./results/error_log_critical.csv"
mv $DIFFFILE "./results/diff_log_critical.txt"

rm ./results/par_output.txt