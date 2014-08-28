obj-y := ktsan.o thr.o shadow.o access.o slab.o sync.o clk.o tab.o report.o stat.o tests.o alloc.o

CFLAGS_REMOVE_ktsan.o = -pg -O0 -O1 -O2
CFLAGS_ktsan.o = -O3

CFLAGS_REMOVE_tests.o = -pg -O1 -O2 -O3
CFLAGS_tests.o = -O0
CFLAGS_tests.o = -fsanitize=thread
