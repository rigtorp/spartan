CXXFLAGS ?= -pipe -g -march=native -std=c++14 -O3 -Wall -z now 
#CXXFLAGS ?= -pipe -g -march=native -std=c++14 -Wall 
LDFLAGS ?= -lrt -pthread

TARGETS = itch log log_bench

all: $(TARGETS)

clean:
	rm -f *~ \#*\# core
	rm -f $(TARGETS)

