#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <random>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include "mpi.h"

const long   SIZE = 64;

long random(const int &min, const int &max) {
	static std::mt19937 generator(117);
	std::uniform_int_distribution<long> distribution(min,max);
	return distribution(generator);
};

void init(auto& M, const long c1, const long c2, const long key) {
	for (long i=0;i<c1;++i)
		for (long j=0;j<c2;++j)
			M[i*c2+j] = (key-i-j)/static_cast<double>(SIZE);
}

// Sequential matrix multiplication:  C = A x B  A[c1][c2] B[c2][c1] C[c1][c1]
// seq_mm returns the sum of the elements of the C matrix.
auto seq_mm(const auto& A, const auto& B, const long c1,const long c2) {

	double sum{0};
	
	for (long i = 0; i < c1; i++) {
		for (long j = 0; j < c1; j++) {
			auto accum = double(0.0);
			for (long k = 0; k < c2; k++)
				accum += A[i*c2+k] * B[k*c1+j];
			sum += accum;
		}
	}
	return sum;
}

// Sequential computation:
// Initialize two matrices with the computed values of the keys
// and execute a matrix multiplication between the two matrices
// to obtain the sum of the elements of the result matrix.
double seq_compute(const long c1, const long c2, long key1, long key2) {

	double *A = new double[c1*c2];
	double *B = new double[c2*c1];

	init(A, c1, c2, key1);
	init(B, c2, c1, key2);
	auto r = seq_mm(A,B, c1,c2);

	delete [] A;
	delete [] B;

	return r;
}

// Parallel multiplication, splitting the computation by rows:
// Initialize two matrices with the computed values of the keys
// and execute in parallel a matrix multiplication between the two matrices.
// The rows of the first matrix are split among the processes,
// each process sums the computed elements and sends the partial sum
// to a master process.
double par_mm(MPI_Comm comm, const long c1, const long c2, long key1, long key2) {
	int numP, rank;
	// get the number of processes
	MPI_Comm_size(comm, &numP);
	// get the rank of the process
	MPI_Comm_rank(comm, &rank);

	double *A = nullptr;
	// B is replicated in all the processes
	double *B = new double[c2*c1];

	// only process 0 inits the data
	if (!rank){
		A = new double[c1*c2];
		init(A, c1, c2, key1);
		init(B, c2, c1, key2);
	}

	// the computation is divided by rows
	int blockRows = c1/numP;
	int myRows = blockRows;

	// for the cases that 'rows' is not multiple of numP
	if(rank < c1%numP){
		myRows++;
	}

	// arrays for the chunk of data to work
	double *myA = new double[myRows*c2];

	// process 0 must specify how many rows are sent to each process
	int *sendCounts = nullptr;
	int *displs = nullptr;
	if (!rank){
		sendCounts = new int[numP];
		displs = new int[numP];

		displs[0] = 0;

		for (int i=0; i<numP; i++){

			if (i>0){
				displs[i] = displs[i-1]+sendCounts[i-1];
			}

			if (i < c1%numP){
				sendCounts[i] = (blockRows+1)*c2;
			} else {
				sendCounts[i] = blockRows*c2;
			}
		}
	}

	// scatter the input matrix A
	MPI_Scatterv(A, sendCounts, displs, MPI_DOUBLE, myA, myRows*c2, MPI_DOUBLE, 0, comm);

	// broadcast the input matrix B
	MPI_Bcast(B, c2*c1, MPI_DOUBLE, 0, comm);

	// multiplication of the submatrices
	double mySum = 0;
	for (long i=0; i<myRows; i++){
		for (long j=0; j<c1; j++){
			auto accum = double(0);
			for (long l=0; l<c2; l++){
				accum += myA[i*c2+l]*B[l*c1+j];
			}
			mySum += accum;
		}
	}

	double r = 0;

	// reduction of partial sums
	MPI_Reduce(&mySum, &r, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

	if (!rank){
		delete [] A;
	}

	delete [] B;
	delete [] myA;

	return r;
}

// Parallel computation:
// Invokes the parallel computation.
double par_compute(MPI_Comm comm, const long c1, const long c2, long key1, long key2) {
	int numP;
	// get the number of processes
	MPI_Comm_size(comm, &numP);

	auto r = par_mm(comm, c1, c2, key1, key2);

	return r;
}

// function to serialize a map into a vector of pairs
std::vector<std::pair<long, long>> serialize_map(const std::map<long, long>& map) {
	return std::vector<std::pair<long, long>>(map.begin(), map.end());
}

// function to deserialize a vector of pairs into a map
std::map<long, long> deserialize_map(const std::vector<std::pair<long, long>>& vec) {
	return std::map<long, long>(vec.begin(), vec.end());
}

int main(int argc, char* argv[]) {
	// initialize MPI
	MPI_Init(&argc, &argv);

	// measure the current time
	MPI_Barrier(MPI_COMM_WORLD);
	double start_time = MPI_Wtime();

	int numP;
	int rank_world;
	// get the rank of the process
	MPI_Comm_rank(MPI_COMM_WORLD, &rank_world);
	// get the number of processes
	MPI_Comm_size(MPI_COMM_WORLD, &numP);

	// matrix size threshold for parallel computation
	long par_comp_threshold = SIZE*SIZE;

	if (argc < 4) {
		if(!rank_world){
			std::printf("use: %s nkeys length [print(0|1) [t]]\n", argv[0]);
			std::printf("     print: 0 disabled, 1 enabled\n");
			std::printf("         t: if matrix size <= t matrix multiplication is executed sequentially\n");
			std::printf("            (default %ld)\n", par_comp_threshold);
		}
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	if (numP < 2) {
		if (!rank_world){
			std::printf("ERROR: The number of processes must be at least 2\n");
		}
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	
	long nkeys  = std::stol(argv[1]);  // total number of keys
	// length is the "stream length", i.e., the number of random key pairs
	// generated
	long length = std::stol(argv[2]);  
	bool print = false;
	if (argc >= 4)
		print = (std::stoi(argv[3]) == 1) ? true : false;
	if (argc == 5)
		par_comp_threshold = std::stoi(argv[4]);

	long key1, key2;
	std::map<long, long> map;
	std::vector<double> V;

	if (!rank_world) { // only the root process initializes the map and the vector
		V.resize(nkeys, 0.0);
		for (long i=0;i<nkeys; ++i) {
			map[i]=0;
		}
	}

	bool resetkey1 = false;
	bool resetkey2 = false;

	MPI_Comm comm, comm_sub;
	// split processes in two groups
	MPI_Comm_split(MPI_COMM_WORLD, rank_world % 2, rank_world, &comm_sub);
	int rank, rank_sub;
	// get rank in sub-communicator
	MPI_Comm_rank(comm_sub, &rank_sub);

	for (int i=0;i<length; ++i) {
		long c1=0, c2=0;
		int split = -1;

		if (!rank_world) { // only the root process generates the keys and updates the map
			key1 = random(0, nkeys-1);  // value in [0,nkeys[
			key2 = random(0, nkeys-1);  // value in [0,nkeys[
			
			if (key1 == key2) // only distinct values in the pair
				key1 = (key1+1) % nkeys; 

			map[key1]++;  // count the number of key1 keys
			map[key2]++;  // count the number of key2 keys

			if (map[key1] == SIZE && map[key2] == SIZE) {
				// processes split in two groups
				split = 2;
			}
			else if (map[key1] == SIZE && map[key2]!=0) {
				// all processes compute key1
				split = 0;
			}
			else if (map[key2] == SIZE && map[key1]!=0) {
				// all processes compute key2
				split = 1;
			}

			c1 = map[key1];
			c2 = map[key2];
		}

		// broadcast the values of split, c1, c2, key1, key2
		double buffer[5];
		if (!rank_world) {
			buffer[0] = (double) split;
			buffer[1] = (double) c1;
			buffer[2] = (double) c2;
			buffer[3] = (double) key1;
			buffer[4] = (double) key2;
		}
		MPI_Bcast(buffer, 5, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		split = (int) buffer[0];
		c1 = (long) buffer[1];
		c2 = (long) buffer[2];
		key1 = (double) buffer[3];
		key2 = (double) buffer[4];

		if (split == -1) // skip computation
			continue;

		int color;
		if (split == 2) { // split in two groups
			comm = comm_sub;
			rank = rank_sub;
			color = rank_world % 2;
		}
		else { // all processes in the same group
			comm = MPI_COMM_WORLD;
			rank = rank_world;
			color = split;
		}

		double r1=0, r2=0;
		
		if (color == 0) {
			if ((c1*c2) <= par_comp_threshold) { // sequential computation
				if (!rank)
					r1 = seq_compute(c1, c2, key1, key2);
				MPI_Barrier(comm);
			} else { // parallel computation
				r1 = par_compute(comm, c1, c2, key1, key2);
			}
			if (split!=2 && !rank_world) { // sum the partial values for key1
				V[key1] += r1;
				resetkey1 = true;
			} else if (!rank) { // send the result to process 0
				MPI_Send(&r1, 1, MPI_DOUBLE, 0, color, MPI_COMM_WORLD); 
			}
		} else {
			if ((c1*c2) <= par_comp_threshold) { // sequential computation
				if (!rank)
					r2 = seq_compute(c2, c1, key2, key1);
				MPI_Barrier(comm);
			} else { // parallel computation
				r2 = par_compute(comm, c2, c1, key2, key1);
			}
			if (split!=2 && !rank_world) { // sum the partial values for key2
				V[key2] += r2;
				resetkey2 = true;
			} else if (!rank) { // send the result to process 0
				MPI_Send(&r2, 1, MPI_DOUBLE, 0, color, MPI_COMM_WORLD);
			}
		}

		if (split==2 && !rank_world) { // collect the results
			MPI_Recv(&r1, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			V[key1] += r1;  // sum the partial values for key1
			MPI_Recv(&r2, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			V[key2] += r2;  // sum the partial values for the other key
			resetkey1 = true;
			resetkey2 = true;
		}

		if (resetkey1 && !rank_world) {
			// updating the map[key1] initial value before restarting
			// the computation
			auto _r1 = static_cast<unsigned long>(r1) % SIZE;
			map[key1] = (_r1>(SIZE/2)) ? 0 : _r1;
			resetkey1 = false;
		}
		if (resetkey2 && !rank_world) {
			// updating the map[key2] initial value before restarting
			// the computation
			auto _r2 = static_cast<unsigned long>(r2) % SIZE;
			map[key2] = (_r2>(SIZE/2)) ? 0 : _r2;
			resetkey2 = false;
		}
	}

	// compute the last values

	std::vector<double> local_V(nkeys, 0.0); // local partial results
	std::vector<std::pair<long, long>> serialized_map(nkeys); // serialized map

	if (!rank_world) {
		// serialize the map
		serialized_map = serialize_map(map);
	}
	// broadcast the serialized map
	MPI_Bcast(serialized_map.data(), nkeys*sizeof(std::pair<long,long>), MPI_BYTE, 0, MPI_COMM_WORLD);
	if (rank_world != 0) {
		// deserialize the map
		map = deserialize_map(serialized_map);
	}

	// each process computes a chunk of the total iterations
	long total_iterations = nkeys * nkeys;
	long chunk_size = (total_iterations + numP - 1) / numP; // ceil(total_iterations / numP)
	long start = rank_world * chunk_size;
	long end = std::min(start + chunk_size, total_iterations);

	for (long idx = start; idx < end; ++idx) {
		long i = idx / nkeys;
		long j = idx % nkeys;
		if (i == j) continue;
			if (map[i] > 0 && map[j] > 0) {
				auto r1 = seq_compute(map[i], map[j], i, j);
				auto r2 = seq_compute(map[j], map[i], j, i);
				local_V[i] += r1;
				local_V[j] += r2;
			}
	}

	// reduce results from all processes into local_V on process 0
	if (!rank_world) {
		MPI_Reduce(MPI_IN_PLACE, local_V.data(), nkeys, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Reduce(local_V.data(), nullptr, nkeys, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	}

	// sum the reduced results from the last loop to the previous results
	if (!rank_world) {
		for (long i = 0; i < nkeys; ++i) {
			V[i] += local_V[i];
		}
	}

	// measure the current time
	double end_time = MPI_Wtime();

	if (!rank_world) {
		// printing the results
		if (print) {
			for (long i=0;i<nkeys; ++i) {
				std::printf("key %ld : %lf\n", i, V[i]);
			}
		}
		// writing log information
		std::printf("%d,%ld,%ld,%ld,%lf\n",
			numP, nkeys, length, par_comp_threshold, end_time-start_time);
	}

	// terminate MPI
	MPI_Comm_free(&comm_sub);
	MPI_Finalize();
	return 0;

}