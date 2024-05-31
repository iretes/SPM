#include <omp.h>
#include <cstring>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>

#define LOG_FILE "./results/word_count_log.csv" // log file name

#ifndef DEBUG
	#define DEBUG 0
#endif

#define DEBUG_PRINT(fmt, ...)\
	if (DEBUG) {{\
		std::printf("(current time = %fs) " fmt,\
			std::chrono::duration<double>(\
				std::chrono::system_clock::now().time_since_epoch()\
			).count(),\
			##__VA_ARGS__);\
	}}

using umap=std::unordered_map<std::string, uint64_t>;
using pair=std::pair<std::string, uint64_t>;
struct Comp {
	bool operator ()(const pair& p1, const pair& p2) const {
		return p1.second > p2.second;
	}
};
using ranking=std::multiset<pair, Comp>;

// ------ globals --------
uint64_t total_words{0};
volatile uint64_t extraworkXline{0};
// ----------------------

void tokenize_line(const std::string& line, umap& UM) {
	char *tmpstr;
	char *token = strtok_r(const_cast<char*>(line.c_str()), " \r\n", &tmpstr);
	while(token) {
		#pragma omp critical
		{
			++UM[std::string(token)];
			++total_words;
		}
		token = strtok_r(NULL, " \r\n", &tmpstr);
	}
	for(volatile uint64_t j{0}; j<extraworkXline; j++);
}

void compute_file(const std::string& filename, umap& UM) {
	std::ifstream file(filename, std::ios_base::in);
	if (file.is_open()) {
		std::string line;
		while(std::getline(file, line)) {
			if (!line.empty()) {
				#pragma omp task shared(UM)
				{
					DEBUG_PRINT("Thread %d processing line '%s' of file '%s'\n",
						omp_get_thread_num(), line.c_str(), filename.c_str());
					tokenize_line(line, UM);
				}
			}
		}
	} 
	file.close();
}

int main(int argc, char *argv[]) {

	auto usage_and_exit = [argv]() {
		std::printf("use: %s filelist.txt [numthreads [extraworkXline [topk [showresults]]]]\n", argv[0]);
		std::printf("     filelist.txt contains one txt filename per line\n");
		std::printf("     numthreads is the number of threads to use\n");
		std::printf("     extraworkXline is the extra work done for each line, it is an integer value whose default is 0\n");
		std::printf("     topk is an integer number, its default value is 10 (top 10 words)\n");
		std::printf("     showresults is 0 or 1, if 1 the output is shown on the standard output\n\n");
		exit(-1);
	};

	std::vector<std::string> filenames;
	uint64_t numthreads = omp_get_max_threads();
	size_t topk = 10;
	bool showresults=false;
	if (argc < 2 || argc > 6) {
		usage_and_exit();
	}

	if (argc > 2) {
		try { numthreads = std::stoul(argv[2]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[2], ex.what());
			return -1;
		}
		if (numthreads == 0) {
			std::printf("%s must be a positive integer\n", argv[2]);
			return -1;
		}

		if (argc > 3) {
			try { extraworkXline=std::stoul(argv[3]);
			} catch(std::invalid_argument const& ex) {
				std::printf("%s is an invalid number (%s)\n", argv[3], ex.what());
				return -1;
			}
			if (argc > 4) {
				try { topk=std::stoul(argv[4]);
				} catch(std::invalid_argument const& ex) {
					std::printf("%s is an invalid number (%s)\n", argv[4], ex.what());
					return -1;
				}
				if (topk==0) {
					std::printf("%s must be a positive integer\n", argv[4]);
					return -1;
				}
				if (argc == 6) {
					int tmp;
					try { tmp=std::stol(argv[5]);
					} catch(std::invalid_argument const& ex) {
						std::printf("%s is an invalid number (%s)\n", argv[5], ex.what());
						return -1;
					}
					if (tmp == 1) showresults = true;
				}
			}
		}
	}
	
	if (std::filesystem::is_regular_file(argv[1])) {
		std::ifstream file(argv[1], std::ios_base::in);
		if (file.is_open()) {
			std::string line;
			while(std::getline(file, line)) {
				if (std::filesystem::is_regular_file(line))
					filenames.push_back(line);
				else
					std::cout << line << " is not a regular file, skipt it\n";
			}					
		} else {
			std::printf("ERROR: opening file %s\n", argv[1]);
			return -1;
		}
		file.close();
	} else {
		std::printf("%s is not a regular file\n", argv[1]);
		usage_and_exit();
	}

	// used for storing results
	umap UM;

	// start the time
	auto start = omp_get_wtime();

	#pragma omp parallel num_threads(numthreads)
	{
		#pragma omp single
		{
			#pragma omp taskloop
			for (auto f : filenames) {
				compute_file(f, UM);
			}
		}
	}

	auto stop1 = omp_get_wtime();
	
	// sorting in descending order
	ranking rank(UM.begin(), UM.end());

	auto stop2 = omp_get_wtime();

	// write the execution times to a file
	std::ofstream file;
	file.open(LOG_FILE, std::ios_base::app);
	file << extraworkXline << "," << numthreads << "," << 
		stop1-start << "," << stop2-stop1 << "\n";
	file.close();
	
	if (showresults) {
		// show the results
		std::cout << "Unique words " << rank.size() << "\n";
		std::cout << "Total words  " << total_words << "\n";
		std::cout << "Top " << topk << " words:\n";
		auto top = rank.begin();
		for (size_t i=0; i < std::clamp(topk, 1ul, rank.size()); ++i)
			std::cout << top->first << '\t' << top++->second << '\n';
	}
}
	
