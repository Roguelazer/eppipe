SOURCES := $(wildcard *.c *.h)
TARGETS := eppipe
PUPPET_TARGETS := eppipe-32 eppipe-64

CFLAGS += -Wall -ggdb -Wextra -pedantic -std=c99 

all: $(TARGETS)

puppet: $(PUPPET_TARGETS)

.PHONY : force
force: clean all

.PHONY : clean 
clean:
	rm -f $(TARGETS) $(PUPPET_TARGETS) $(TEST_TARGETS) *.o
	rm -rf build

eppipe-32: $(SOURCES)
	$(CC) -o $@ $(CFLAGS) -m32 $(filter %.c,$^)

eppipe-64: $(SOURCES)
	$(CC) -o $@ $(CFLAGS) -m64 $(filter %.c,$^)

dep:
	sh ./automake.sh

#### AUTO ####
#### END AUTO ####
