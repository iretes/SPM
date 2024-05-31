#!/bin/sh
#SBATCH -p normal
#SBATCH -N 1
#SBATCH -o ./results/slurm_1node.log
#SBATCH -e ./results/slurm_1node.err

REPETITIONS=10
PAR_STRONG_LOGFILE=./results/par_timing_strong_1node.csv
PAR_WEAK_LOGFILE=./results/par_timing_weak_1node.csv 

# clear log files
truncate -s 0 $PAR_STRONG_LOGFILE
truncate -s 0 $PAR_WEAK_LOGFILE

# strong scaling

for ntasks in 2 4 8 16 32; do
    for nkeys in 2 100 500; do
        for threshold in 0 2048 4096; do
            for rep in $(seq 1 $REPETITIONS); do
                echo "[$rep/$REPETITIONS] -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold"
                srun --mpi=pmix -n $ntasks ./nkeys_par $nkeys 1000000 0 $threshold >> $PAR_STRONG_LOGFILE
            done
        done
    done
done

# weak scaling

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