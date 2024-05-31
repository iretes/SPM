CXX                = mpicxx -std=c++20
OPTFLAGS	   = -O3 -ffast-math
CXXFLAGS          += -Wall
INCLUDES	   = -I. -I./include
LIBS               = 
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

ifdef DEBUG
CXXFLAGS += -DDEBUG
endif

.PHONY: all debug clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)

debug:
	make cleanall --no-print-directory;\
	make all DEBUG=1 --no-print-directory;

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
	-rm -f *.txt



