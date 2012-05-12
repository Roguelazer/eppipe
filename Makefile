SOURCES := $(wildcard *.cc)
TARGETS := eppipe
PUPPET_TARGETS := eppipe-32 eppipe-64

CFLAGS += -Wall -g -Wextra -pedantic -pthread -std=c++11
LDFLAGS = -lboost_system

all: $(TARGETS)

puppet: $(PUPPET_TARGETS)

.PHONY : force
force: clean all

.PHONY : clean 
clean:
	rm -f $(TARGETS) $(PUPPET_TARGETS) $(TEST_TARGETS) *.o
	rm -rf build

eppipe-32: $(SOURCES)
	$(CXX) -o $@ $(CFLAGS) $(LDFLAGS) -m32 $(filter %.cc,$^)

eppipe-64: $(SOURCES)
	$(CXX) -o $@ $(CFLAGS) $(LDFLAGS) -m64 $(filter %.cc,$^)

dep:
	sh ./automake.sh

#### AUTO ####
eppipe.o: eppipe.cc
#### END AUTO ####
