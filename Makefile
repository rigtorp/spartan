CXXFLAGS ?= -pipe -g -march=native -std=c++14 -O3 -Wall -z now 
#CXXFLAGS ?= -pipe -g -march=native -std=c++14 -Wall 
LDFLAGS ?= -lrt 

TARGETS = itch log log2

all: $(TARGETS)

clean:
	rm -f *~ \#*\# core
	rm -f $(TARGETS)

