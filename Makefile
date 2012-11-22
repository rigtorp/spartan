CXXFLAGS ?= -pipe -g -march=native -std=c++11 -O3 -Wall -z now 
#CXXFLAGS ?= -pipe -g -march=native -std=c++11 -Wall 
LDFLAGS ?= -lrt 

TARGETS = itch

all: $(TARGETS)

clean:
	rm -f *~ \#*\# core
	rm -f $(TARGETS)

