obj-y := ktsan.o	\
	 thr.o		\
	 shadow.o	\
	 access.o	\
	 memblock.o	\
	 sync.o		\
	 clk.o		\
	 tab.o		\
	 report.o	\
	 stat.o		\
	 alloc.o	\
	 atomic.o	\
	 stack.o	\
	 trace.o	\
	 tests.o	

CFLAGS_REMOVE_ktsan.o = -pg -O0 -O1 -O2
CFLAGS_ktsan.o = -O3

CFLAGS_REMOVE_tests.o = -pg -O1 -O2 -O3
CFLAGS_tests.o = -O0

KTSAN_SANITIZE_ktsan.o = n
KTSAN_SANITIZE_thr.o = n
KTSAN_SANITIZE_shadow.o = n
KTSAN_SANITIZE_access.o = n
KTSAN_SANITIZE_memblock.o = n
KTSAN_SANITIZE_sync.o = n
KTSAN_SANITIZE_clk.o = n
KTSAN_SANITIZE_tab.o = n
KTSAN_SANITIZE_report.o = n
KTSAN_SANITIZE_stat.o = n
KTSAN_SANITIZE_alloc.o = n
KTSAN_SANITIZE_atomic.o = n
KTSAN_SANITIZE_stack.o = n
KTSAN_SANITIZE_trace.o = n

KTSAN_SANITIZE_tests.o = y
