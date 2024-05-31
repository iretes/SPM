mkdir ./results/

######################### LOGGING PERFORMED OPERATIONS #########################

echo "Running sequential version while logging operations..."

make debug

./nkeys_seq 2 1000000 > ./results/seq_2_1000000_log.csv
./nkeys_seq 100 1000000 > ./results/seq_100_1000000_log.csv

# delete the last line from the files containing execution times
sed -i '$ d' ./results/seq_2_1000000_log.csv
sed -i '$ d' ./results/seq_100_1000000_log.csv

########################### CORRECTNESS VERIFICATION ###########################

echo "Recompiling the code without debugging flag..."
make cleanall
make

echo "Running sequential version saving the output...."
./nkeys_seq 2 1000000 1 > ./results/seq_2_1000000_output.csv
./nkeys_seq 100 1000000 1 > ./results/seq_100_1000000_output.csv

echo "Running parallel version saving the output...."
mpirun -n 16 ./nkeys_par 2 1000000 1 > ./results/par_2_1000000_output.csv
mpirun -n 16 ./nkeys_par 100 1000000 1 > ./results/par_100_1000000_output.csv

# delete the last line from the files containing execution times
sed -i '$ d' ./results/seq_2_1000000_output.csv
sed -i '$ d' ./results/seq_100_1000000_output.csv
sed -i '$ d' ./results/par_2_1000000_output.csv
sed -i '$ d' ./results/par_100_1000000_output.csv

echo "Comparing the output of the sequential and parallel versions..."
diff ./results/seq_2_1000000_output.csv ./results/par_2_1000000_output.csv > ./results/diff_seq_par_2_1000000.txt
diff ./results/seq_100_1000000_output.csv ./results/par_100_1000000_output.csv > ./results/diff_seq_par_100_1000000.txt

############################# PERFORMANCE EVALUATION ###########################

echo "Running on the cluster..."
sbatch slurm_script_8nodes.sh
sbatch slurm_script_1node.sh