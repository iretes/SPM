# 
# FF_ROOT     points to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT 
FF_ROOT		= ${HOME}/fastflow
endif

CXX                = g++ -std=c++20
OPTFLAGS	   = -O3 -ffast-math -march=native
CXXFLAGS          += -Wall

ifdef NO_DEFAULT_MAPPING
CXXFLAGS += -DNO_DEFAULT_MAPPING
endif
ifdef BLOCKING_MODE
CXXFLAGS += -DBLOCKING_MODE
endif
ifdef BOUNDED_BUFFER
CXXFLAGS += -DFF_BOUNDED_BUFFER
endif

INCLUDES	   = -I. -I./include -I $(FF_ROOT)
LIBS               = -pthread -fopenmp
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~

cleanall: clean
	-rm -fr $(TARGET)
