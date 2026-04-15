# krep - A high-performance string search utility
# Author: Davide Santangelo
# Version: 0.4.2

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

CC ?= gcc
BASE_CFLAGS += -O1 -fno-tree-vectorize -fno-tree-slp-vectorize -fno-fast-math -D_GNU_SOURCE -D_DEFAULT_SOURCE
CFLAGS += $(BASE_CFLAGS)

LDFLAGS += $(BASE_LDFLAGS)

# Detect architecture for SIMD flags (basic example)
ARCH := $(shell uname -m)


# Source files
SRCS = krep.c aho_corasick.c
OBJS = $(SRCS:.c=.o)

# Test source files
TEST_SRCS = test/test_krep.c test/test_regex.c test/test_multiple_patterns.c
TEST_OBJS_MAIN = krep_test.o aho_corasick_test.o # Specific objects for test build
TEST_OBJS_TEST = $(TEST_SRCS:.c=.o)
TEST_TARGET = krep_test

TARGET = krep

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule for main objects
%.o: %.c krep.h aho_corasick.h
	$(CC) $(CFLAGS) -c $< -o $@

# --- Test Build ---
# Rule for test-specific main objects (compiled with -DTESTING)
krep_test.o: krep.c krep.h aho_corasick.h
	$(CC) $(CFLAGS) -DTESTING -c krep.c -o krep_test.o

aho_corasick_test.o: aho_corasick.c krep.h aho_corasick.h
	$(CC) $(CFLAGS) -DTESTING -c aho_corasick.c -o aho_corasick_test.o

# Rule for test file objects (compiled with -DTESTING)
test/%.o: test/%.c test/test_krep.h test/test_compat.h krep.h
	$(CC) $(CFLAGS) -DTESTING -c $< -o $@

# Link test executable
$(TEST_TARGET): $(TEST_OBJS_MAIN) $(TEST_OBJS_TEST)
	$(CC) $(CFLAGS) -DTESTING -o $(TEST_TARGET) $(TEST_OBJS_MAIN) $(TEST_OBJS_TEST) $(LDFLAGS) -lm # Add -lm if needed

test: $(TEST_TARGET)
	./$(TEST_TARGET)

test_directory: test/test_directory.c krep.c aho_corasick.c
	$(CC) $(CFLAGS) -DTESTING -o $@ $^ $(LDFLAGS)

all-tests: test_basic test_krep test_regex test_multiple_patterns test_directory

# --- Installation ---
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# --- Cleanup ---
clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJS) $(TEST_OBJS_MAIN) $(TEST_OBJS_TEST) *.o test/*.o
