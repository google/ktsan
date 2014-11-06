obj-y := ktsan.o		\
	 thr.o			\
	 shadow.o		\
	 access.o		\
	 memblock.o		\
	 sync.o			\
	 sync_atomic.o		\
	 sync_percpu.o		\
	 clk.o			\
	 tab.o			\
	 report.o		\
	 stat.o			\
	 alloc.o		\
	 atomic_no_ktsan.o	\
	 stack.o		\
	 trace.o		\
	 func.o			\
	 lock.o			\
	 tests_noinst.o		\
	 tests_inst.o		\
	 tests.o

CFLAGS_REMOVE_ktsan.o = -pg -O0 -O1 -O2
CFLAGS_ktsan.o = -O3

CFLAGS_REMOVE_tests_inst.o = -pg -O1 -O2 -O3
CFLAGS_tests_inst.o = -O0

KTSAN_SANITIZE := n

KTSAN_SANITIZE_tests_inst.o = y
