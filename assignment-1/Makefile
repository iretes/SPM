CXX                = g++ -std=c++20
OPTFLAGS	       = -O3 -march=native
CXXFLAGS          += -Wall
ifeq ($(DEBUG),1)
	CXXFLAGS      += -DDEBUG
endif
INCLUDES	       = -I. -I./include
LIBS               = -pthread
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