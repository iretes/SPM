#!/bin/sh
#SBATCH -p normal
#SBATCH -N 8
#SBATCH -o ./results/slurm_8nodes.log
#SBATCH -e ./results/slurm_8nodes.err

REPETITIONS=10
PAR_STRONG_LOGFILE=./results/par_timing_strong_8nodes.csv
SEQ_STRONG_LOGFILE=./results/seq_timing_strong.csv
PAR_WEAK_LOGFILE=./results/par_timing_weak_8nodes.csv
SEQ_WEAK_LOGFILE=./results/seq_timing_weak.csv

# clear log files
truncate -s 0 $PAR_STRONG_LOGFILE
truncate -s 0 $SEQ_STRONG_LOGFILE
truncate -s 0 $PAR_WEAK_LOGFILE
truncate -s 0 $SEQ_WEAK_LOGFILE

# parallel execution - strong scaling

for ntasks in 2 4 8 16 32 64; do
    for nkeys in 2 100; do
        for threshold in 0 4096; do
            for rep in $(seq 1 $REPETITIONS); do
                echo "[$rep/$REPETITIONS] -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold"
                srun --mpi=pmix -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold >> $PAR_STRONG_LOGFILE
            done
        done
    done
done

# parallel execution - weak scaling

ntasks_list=(2 4 8 16 32)
nkeys_list=(50 100 200 400 800)

for i in "${!ntasks_list[@]}"; do
    ntasks=${ntasks_list[$i]}
    nkeys=${nkeys_list[$i]}
    for threshold in 0 2048 4096; do
        for rep in $(seq 1 $REPETITIONS); do
            echo "[$rep/$REPETITIONS] -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold"
            srun --mpi=pmix -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold >> $PAR_WEAK_LOGFILE
        done
    done
done

# sequential execution - strong scaling

for nkeys in 2 100 500; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] ./nkeys $nkeys 1000000 0"
        srun ./nkeys_seq $nkeys 1000000 0 >> $SEQ_STRONG_LOGFILE
    done
done

# sequential execution - weak scaling

for nkeys in 50 100 200 400 800; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "[$rep/$REPETITIONS] ./nkeys $nkeys 1000000 0"
        srun ./nkeys_seq $nkeys 1000000 0 >> $SEQ_WEAK_LOGFILE
    done
done
