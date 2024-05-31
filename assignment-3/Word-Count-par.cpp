#include <cstring>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <ff/ff.hpp>

using namespace ff;

#define LOG_FILE "./results/word_count_log.csv" // log file name

using umap=std::unordered_map<std::string, uint64_t>;
using pair=std::pair<std::string, uint64_t>;
struct Comp {
	bool operator ()(const pair& p1, const pair& p2) const {
		return p1.second > p2.second;
	}
};
using ranking=std::multiset<pair, Comp>;

// ------ globals --------
std::atomic<uint64_t> total_words{0};
volatile uint64_t extraworkXline{0};
// ----------------------

struct FileReader : ff_monode_t<std::string> {
	FileReader(
		const std::vector<std::string> &filenames_,
		const uint64_t Lw_
	) : filenames(filenames_), Lw(Lw_) {}

	std::string* svc(std::string*) {
		uint64_t id = get_my_id();
		uint64_t num_files = filenames.size();

		for (uint64_t i=id; i<num_files; i+=Lw) {
			std::ifstream file(filenames[i], std::ios_base::in);
			if (file.is_open()) {
				std::string line;
				while(std::getline(file, line)) {
					if (!line.empty()) {
						ff_send_out(new std::string(line));
					}
				}

			} 
			file.close();
		}

		return EOS;
	}
	
	const std::vector<std::string> &filenames;
	const uint64_t Lw;
};

struct Tokenizer : ff_minode_t<std::string> {
	Tokenizer(umap &um_) : um(um_) {}

	std::string* svc(std::string* line) {
		char *tmpstr;
		char *token = strtok_r(const_cast<char*>(line->c_str()), " \r\n", &tmpstr);
		while(token) {
			um[std::string(token)]++;
			token = strtok_r(NULL, " \r\n", &tmpstr);
			total_words++;
		}
		for(volatile uint64_t j {0}; j < extraworkXline; j++);

		delete line;
		return GO_ON;
	}

	umap &um;
};

int main(int argc, char *argv[]) {

	auto usage_and_exit = [argv]() {
		std::printf("use: %s filelist.txt [Lw [Rw [on-demand [extraworkXline [topk [showresults]]]]]]\n", argv[0]);
		std::printf("     filelist.txt contains one txt filename per line\n");
		std::printf("     Lw is the number of left workers to use\n");
		std::printf("     Rw is the number of right workers to use\n");
		std::printf("     on-demand is the value of the parameter 'ondemand' in the add_firstset method\n"
					"               of the building block ff_a2a, its default value is 0\n");
		std::printf("     extraworkXline is the extra work done for each line, it is an integer value whose default is 0\n");
		std::printf("     topk is an integer number, its default value is 10 (top 10 words)\n");
		std::printf("     showresults is 0 or 1, if 1 the output is shown on the standard output\n\n");
		exit(-1);
	};

	std::vector<std::string> filenames;
	uint64_t Lw = 1;
	uint64_t Rw = ff_numCores() - 1;
	int ondemand = 0;
	size_t topk = 10;
	bool showresults = false;
	if (argc < 2 || argc > 8) {
		usage_and_exit();
	}
	if (argc > 2) {
		try { Lw = std::stoul(argv[2]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[2], ex.what());
			return -1;
		}
		if (Lw == 0) {
			std::printf("%s must be a positive integer\n", argv[2]);
			return -1;
		}
	}
	if (argc > 3) {
		try { Rw = std::stoul(argv[3]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[3], ex.what());
			return -1;
		}
		if (Rw == 0) {
			std::printf("%s must be a positive integer\n", argv[3]);
			return -1;
		}
	}
	if (argc > 4) {
		try { ondemand = std::stoi(argv[4]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[4], ex.what());
			return -1;
		}
	}
	if (argc > 5) {
		try { extraworkXline=std::stoul(argv[5]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[5], ex.what());
			return -1;
		}
	}
	if (argc > 6) {
		try { topk=std::stoul(argv[6]);
			} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[6], ex.what());
			return -1;
		}
		if (topk==0) {
			std::printf("%s must be a positive integer\n", argv[6]);
			return -1;
		}
	}
	if (argc == 8) {
		int tmp;
		try { tmp=std::stol(argv[7]);
		} catch(std::invalid_argument const& ex) {
			std::printf("%s is an invalid number (%s)\n", argv[7], ex.what());
			return -1;
		}
		if (tmp == 1) showresults = true;
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

	// use 1 left workers if a single file needs to be processed
	Lw = (filenames.size() == 1) ? 1 : Lw;

	// used for storing results
	std::vector<umap> umaps(Rw);

	// start the time
	ffTime(START_TIME);

	std::vector<ff_node*> LW;
	std::vector<ff_node*> RW;

	for (size_t i=0; i<Lw; ++i)
		LW.push_back(new FileReader(filenames, Lw));

	for ( size_t i=0; i<Rw; ++i)
		RW.push_back(new Tokenizer(umaps[i]));
	
	ff_a2a a2a;
	a2a.add_firstset(LW, ondemand);
	a2a.add_secondset(RW);

	if (a2a.run_and_wait_end()<0) {
		error("running a2a\n");
		return -1;
	}

	ffTime(STOP_TIME);
	auto map_time = ffTime(GET_TIME);

	// start the time
	ffTime(START_TIME);

	for (uint64_t id = 1; id < Rw; id++) {
		for (auto& i: umaps[id]) {
			umaps[0][i.first] += i.second;
		}
	}

	ffTime(STOP_TIME);
	auto reduce_time = ffTime(GET_TIME);

	// start the time
	ffTime(START_TIME);
	
	// sorting in descending order
	ranking rank(umaps[0].begin(), umaps[0].end());

	ffTime(STOP_TIME);
	auto rank_time = ffTime(GET_TIME);
	
	std::ofstream file;
	file.open(LOG_FILE, std::ios_base::app);
	file << Lw << "," << Rw << "," << ondemand << "," << extraworkXline << ","
	<< map_time << "," 
	<< reduce_time << "," << rank_time << "\n";
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

	// free memory
	for (size_t i=0; i<Lw; ++i)
		delete LW[i];
	for (size_t i=0; i<Rw; ++i)
		delete RW[i];
}
	
