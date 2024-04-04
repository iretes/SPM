#include <iostream>
#include <unistd.h>
#include <vector>
#include <thread>
#include <random>
#include <barrier>
#include <threadPool.hpp>
#include <fstream>

#ifndef DEBUG
	#define DEBUG 0
#endif

#define DEFAULT_LOG_FILE "wavefront_log.csv" // default log file name
#define DEFAULT_N 512 // default size of the square matrix (NxN)
#define DEFAULT_T 2 // default number of threads
#define DEFAULT_m 0 // default minimum time (in microseconds)
#define DEFAULT_M 1000 // default maximum time (in microseconds)

#define DEBUG_PRINT(start, fmt, ...)                                           \
	if (DEBUG) {{                                                              \
		std::chrono::time_point<std::chrono::system_clock> now;                \
		now = std::chrono::system_clock::now();                                \
		std::chrono::duration<double> delta = now-start;                       \
		std::printf("(elapsed time = %fs) " fmt, delta.count(), ##__VA_ARGS__);\
	}}

#define TIMERSTART(label)                                                      \
	std::chrono::time_point<std::chrono::system_clock> a##label, b##label;     \
	a##label = std::chrono::system_clock::now();

#define TIMERSTOP(label, time_elapsed)                                         \
	b##label = std::chrono::system_clock::now();                               \
	std::chrono::duration<double> delta##label = b##label-a##label;            \
	time_elapsed = delta##label.count();

int random(const int &min, const int &max) {
	static std::mt19937 generator(117);
	std::uniform_int_distribution<int> distribution(min,max);
	return distribution(generator);
};

void work(std::chrono::microseconds w) {
	auto end = std::chrono::steady_clock::now() + w;
	while(std::chrono::steady_clock::now() < end);
}

// parallel wavefront algorithm with static scheduling
void wavefront_parallel_static(
	const std::vector<int> &M,
	const uint64_t &N,
	const uint32_t &T
	) {

		std::barrier bar(T);
		std::chrono::time_point<std::chrono::system_clock> start = 
			std::chrono::system_clock::now();

		auto static_task = [&] (const uint64_t id) -> void {
			for (uint64_t k=0; k<N; ++k) { // for each upper diagonal
				if (id >= N-k) {
					bar.arrive_and_drop();
					DEBUG_PRINT(start, "Thread %lu: exit\n", id)
					return;
				}
				for (uint64_t i=id; i<N-k; i+=T) { // for each assigned elem.
					work(std::chrono::microseconds(M[i*N+(i+k)]));
					DEBUG_PRINT(start,"Thread %lu: computed index %lu\n",
						id, i*N+(i+k))
				}
				bar.arrive_and_wait();
				DEBUG_PRINT(start,"Thread %lu: unlocked from waiting\n", id)
			}
		};

		std::vector<std::thread> threads;
		for (uint64_t id=0; id<T; id++)
			threads.emplace_back(static_task, id);

		for (auto &thread : threads)
			thread.join();
	}

// parallel wavefront algorithm with dynamic scheduling
void wavefront_parallel_dynamic(
	const std::vector<int> &M,
	const uint64_t &N,
	const uint32_t &T
	) {

	std::barrier bar(T);
	std::chrono::time_point<std::chrono::system_clock> start;
	if (DEBUG) start = std::chrono::system_clock::now();

	auto process_element = [&](uint64_t index, bool block) {
		work(std::chrono::microseconds(M[index]));
		if (block) {
			bar.arrive_and_wait();
			DEBUG_PRINT(start, "Computed index %lu and waited\n", index)
		}
		else {
			DEBUG_PRINT(start, "Computed index %lu without waiting\n", index)
		}
	};

	auto wait = [&] () {
		bar.arrive_and_wait();
		DEBUG_PRINT(start, "Waited\n")
	};

	ThreadPool TP(T);
	for (uint64_t k=0; k<N; ++k) { // for each upper diagonal
		if ((N-k)<T) { // if the diagonal is smaller than the number of threads
			for (uint64_t i=(N-k); i<T; ++i) { // for each extra thread
				TP.enqueue(wait);
			}
		}
		for (uint64_t i=0; i<(N-k); ++i) { // for each elem. in the diagonal
			bool block = (i >= (N-k-T) || (N-k)<T) ? true : false;
			TP.enqueue(process_element, i*N+(i+k), block);
		}
	}
}

// sequential wavefront algorithm
void wavefront_sequential(const std::vector<int> &M, const uint64_t &N) {
	for(uint64_t k=0; k< N; ++k) { // for each upper diagonal
		for(uint64_t i=0; i<(N-k); ++i) { // for each elem. in the diagonal
			work(std::chrono::microseconds(M[i*N+(i+k)])); 
		}
	}
}

void print_usage() {
	std::printf("usage: wavefront [options]\n"
				"     -h               prints this message\n"
				"     -N size          size of the square matrix [default=%d]\n"
				"     -T num_threads   number of threads [default=%d]\n"
				"     -m min           min waiting time in us [default=%d]\n"
				"     -M max           max waiting time in us [default=%d]\n"
				"     -l file_name     log file name [default=%s]\n"
				"     -s               whether to execute the sequential\n"
				"                      algorithm [not executed by default]\n",
				DEFAULT_N, DEFAULT_T, DEFAULT_m, DEFAULT_M, DEFAULT_LOG_FILE);
}

int main(int argc, char *argv[]) {
	int min                   = DEFAULT_m;
	int max                   = DEFAULT_M;
	uint64_t N                = DEFAULT_N;
	uint32_t T                = DEFAULT_T;
	bool seq_exec             = false;
	std::string log_file_name = DEFAULT_LOG_FILE;

	int opt;
	while ((opt = getopt(argc, argv, "hN:T:m:M:s:l:")) != -1) {
		switch (opt) {
			case 'h':
				print_usage();
				return 0;
			case 'N':
				N = atoi(optarg);
				break;
			case 'T':
				T = atoi(optarg);
				break;
			case 'm':
				min = atoi(optarg);
				break;
			case 'M':
				max = atoi(optarg);
				break;
			case 's':
				seq_exec = true;
				break;
			case 'l':
				log_file_name = optarg;
				break;
			case '?':
				if (optopt == 'N' ||
					optopt == 'T' ||
					optopt == 'm' ||
					optopt == 'M' ||
					optopt == 'l')
					std::cerr << "Option -" << static_cast<char>(optopt)
						<< " requires an argument.\n";
				else if (isprint(optopt))
					std::cerr << "Unknown option `-" <<
						static_cast<char>(optopt) << "`.\n";
				else
					std::cerr << "Unknown option character `\\x" <<
						std::hex << optopt << "`.\n";
				print_usage();
				return 1;
			default:
				return 1;
		}
	}

	// allocate the matrix
	std::vector<int> M(N*N, -1);

	uint64_t expected_seq_totaltime=0;
	// init function
	auto init=[&]() {
		for(uint64_t k=0; k<N; ++k) {
			for(uint64_t i=0; i<(N-k); ++i) {
				int t = random(min,max);
				M[i*N+(i+k)] = t;
				expected_seq_totaltime +=t;
			}
		}
	};
	
	init();

	// sequential execution
	double actual_seq_totaltime=-1;
	if (seq_exec) {
		if (DEBUG) std::printf("------ Sequential execution ------ \n");
		TIMERSTART(wavefront_sequential);
		wavefront_sequential(M, N); 
		TIMERSTOP(wavefront_sequential, actual_seq_totaltime);
	}
	
	// parallel dynamic execution
	double par_dynamic_totaltime;
	if (DEBUG) std::printf("------ Parallel dynamic execution ------\n");
	TIMERSTART(wavefront_parallel_dynamic);
	wavefront_parallel_dynamic(M, N, T); 
	TIMERSTOP(wavefront_parallel_dynamic, par_dynamic_totaltime);

	// parallel static execution
	double par_static_totaltime;
	if (DEBUG) std::printf("------ Parallel static execution ------\n");
	TIMERSTART(wavefront_parallel_static);
	wavefront_parallel_static(M, N, T); 
	TIMERSTOP(wavefront_parallel_static, par_static_totaltime);

	// write the execution times to a file
	std::ofstream file;
	file.open(log_file_name, std::ios_base::app);
	file << N << "," << T << "," << min << "," << max << ","
		<< expected_seq_totaltime/1000000 << "," << actual_seq_totaltime << ","
		<< par_dynamic_totaltime << "," << par_static_totaltime << "\n";
	file.close();

	return 0;
}