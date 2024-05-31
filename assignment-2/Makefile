CXX                = g++ -std=c++20
OPTFLAGS	   = -O3
CXXFLAGS          += -Wall 
ifeq ($(DEBUG),1)
	CXXFLAGS      += -DDEBUG
endif
AUTOFLAGS          = -march=native -ffast-math -mavx2
INCLUDES	   = -I. -I./include
LIBS               = -pthread -fopenmp
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall debug undebug

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)

debug:
	make cleanall --no-print-directory;\
	make all DEBUG=1 --no-print-directory;

undebug:
	make cleanall --no-print-directory;\
	make all DEBUG=0 --no-print-directory;

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
