#!/bin/bash

TOPK=5
REPETITIONS=10
LOGFILE="./results/word_count_log.csv"
ERRORFILE="./results/error_log.csv"
DIFFFILE="./results/diff_log.txt"

# create the results directory
mkdir ./results/

###################### SAVING FILE SIZES TO A CSV FILE #########################

FILE_SIZES="./results/file_sizes.csv"
echo "KB,file_path" > $FILE_SIZES
while IFS= read -r file_path; do
    size=$(du -sk "$file_path" | awk '{print $1}')
    echo "$size,$file_path" >> $FILE_SIZES
done < /opt/SPMcode/A2/filelist.txt

######################## EXECUTING SEQUENTIAL VERSION ##########################

make

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

############### EXECUTING SEQUENTIAL VERSION WITH BALANCED FILES ###############

# creating 70 copies of a file (sized 508 KB)
mkdir files
for ((i=1; i<=70; i++)); do
    cp /opt/SPMcode/A2/files/pg78.txt ./files/file$i.txt
    echo ./files/file$i.txt >> myfilelist.txt
done

# empty the log file for time measurements
truncate -s 0 $LOGFILE

echo "Executing sequential version with balanced files"
for rep in $(seq 1 $REPETITIONS); do
    echo "[$rep/$REPETITIONS] Word-Count-seq myfilelist.txt 10000 $TOPK 1"
    ./Word-Count-seq myfilelist.txt 10000 $TOPK 1 > /dev/null
done

# rename the log file
mv $LOGFILE "./results/word_count_log_seq_balanced.csv"

########################## TESTING WITH MORE RIGHT WORKERS #####################

# empty the log file for time measurements
truncate -s 0 $LOGFILE
# empty the log file for output differences
truncate -s 0 $DIFFFILE
# write the header in the log file for errors
echo "filelist,lw,rw,ondemand,extraworkXline,topk,result" > $ERRORFILE

threads="2 4 8 12 20 28 36 44 52 60"
lws="1 4 14"

echo "Executing with more right workers"
for lw in $lws; do
    for t in $threads; do
        rw=$((t-lw))
        if [ $rw -gt 0 -a $rw -ge $lw ]; then
            for w in 0 1000 10000; do
                for od in 0 1; do
                    for rep in $(seq 1 $REPETITIONS); do
                        echo "[$rep/$REPETITIONS] Word-Count-par /opt/SPMcode/A2/filelist.txt $lw $rw $od $w $TOPK 1"
                        ./Word-Count-par /opt/SPMcode/A2/filelist.txt $lw $rw $od $w $TOPK 1 > "./results/par_output.txt"
                        DIFF=$(diff "./results/seq_output.txt" "./results/par_output.txt")
                        if [ "$DIFF" != "" ]; then
                            echo "--------" >> $DIFFFILE
                            echo Word-Count-par /opt/SPMcode/A2/filelist.txt $lw $rw $od $w $TOPK 1 >> $DIFFFILE
                            echo $DIFF >> $DIFFFILE
                            echo "--------" >> $DIFFFILE
                            echo /opt/SPMcode/A2/filelist.txt,$lw,$rw,$od,$w,$TOPK,NOK >> $ERRORFILE
                        else
                            echo /opt/SPMcode/A2/filelist.txt,$lw,$rw,$od,$w,$TOPK,OK >> $ERRORFILE
                        fi
                    done
                done
            done
        fi
    done
done

# remove the temporary output file
rm ./results/par_output.txt
# rename the log file
mv $LOGFILE "./results/word_count_log_morerw.csv"

########################## TESTING WITH MORE LEFT WORKERS ######################

# empty the log file for time measurements
truncate -s 0 $LOGFILE

echo "Executing with more left workers"
for w in 0 1000 10000; do
    for t in $threads; do
        lw=$(echo "($t + 2)/3 * 2" | bc)
        rw=$((t-lw))
        if [ $rw -gt 0 -a $lw -gt 0 ]; then
            for rep in $(seq 1 "$REPETITIONS"); do
                echo "[$rep/$REPETITIONS] Word-Count-par /opt/SPMcode/A2/filelist.txt $lw $rw 0 $w $TOPK 1"
                ./Word-Count-par /opt/SPMcode/A2/filelist.txt $lw $rw 0 $w $TOPK 1 > /dev/null
            done
        fi
    done
done

# rename the log file
mv ./results/word_count_log.csv ./results/word_count_log_morelw.csv

######################## TESTING WITH BALANCED FILES ###########################

# empty the log file for time measurements
truncate -s 0 $LOGFILE

echo "Executing parallel version with balanced files"
for rep in $(seq 1 "$REPETITIONS"); do
    echo "[$rep/$REPETITIONS] Word-Count-par myfilelist.txt 1 27 0 10000 $TOPK 1"
    ./Word-Count-par myfilelist.txt 1 27 0 10000 $TOPK 1 > /dev/null
    echo "[$rep/$REPETITIONS] Word-Count-par myfilelist.txt 4 24 0 10000 $TOPK 1"
    ./Word-Count-par myfilelist.txt 4 24 0 10000 $TOPK 1 > /dev/null
    echo "[$rep/$REPETITIONS] Word-Count-par myfilelist.txt 14 14 0 10000 $TOPK 1"
    ./Word-Count-par myfilelist.txt 14 14 0 10000 $TOPK 1 > /dev/null
done

# rename the log file
mv ./results/word_count_log.csv ./results/word_count_log_balanced.csv

######################## TESTING WITHOUT THREAD MAPPING ########################

# empty the log file for time measurements
truncate -s 0 $LOGFILE

# re-compiling the code without the default mapping
make cleanall
make NO_DEFAULT_MAPPING=1

echo "Executing without thread mapping"
for rep in $(seq 1 "$REPETITIONS"); do
    echo "[$rep/$REPETITIONS] Word-Count-par /opt/SPMcode/A2/filelist.txt 14 46 0 10000 $TOPK 1"
    ./Word-Count-par /opt/SPMcode/A2/filelist.txt 14 46 0 10000 $TOPK 1 > /dev/null
done

# rename the log file
mv ./results/word_count_log.csv ./results/word_count_log_nodefaultmap.csv