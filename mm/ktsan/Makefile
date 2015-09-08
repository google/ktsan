obj-y := access.o 		\
	 cache.o 		\
	 clk.o 			\
	 func.o 		\
	 ktsan.o 		\
	 memblock.o 		\
	 report.o 		\
	 shadow.o 		\
	 spinlock.o		\
	 stack.o 		\
	 stack_depot.o 		\
	 stat.o 		\
	 supp.o			\
	 sync_atomic.o 		\
	 sync_atomic_no_ktsan.o \
	 sync_mtx.o 		\
	 sync_percpu.o 		\
	 sync_seqlock.o 	\
	 sync.o 		\
	 tab.o 			\
	 tests_inst.o 		\
	 tests_noinst.o 	\
	 tests.o 		\
	 thr.o 			\
	 trace.o 		\

CFLAGS_REMOVE_access.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_cache.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_clk.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_func.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_ktsan.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_memblock.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_report.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_shadow.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_spinlock.o                = -pg -O0 -O1 -O2
CFLAGS_REMOVE_stack.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_stack_depot.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_stat.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_supp.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync_atomic.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync_atomic_no_ktsan.o	= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync_mtx.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync_percpu.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync_seqlock.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_sync.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_tab.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_tests_inst.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_tests_noinst.o		= -pg -O0 -O1 -O2
CFLAGS_REMOVE_tests.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_thr.o			= -pg -O0 -O1 -O2
CFLAGS_REMOVE_trace.o			= -pg -O0 -O1 -O2

CFLAGS_access.o			= -O3 -fomit-frame-pointer
CFLAGS_cache.o			= -O3 -fomit-frame-pointer
CFLAGS_clk.o			= -O3 -fomit-frame-pointer
CFLAGS_func.o			= -O3 -fomit-frame-pointer
CFLAGS_ktsan.o			= -O3 -fomit-frame-pointer
CFLAGS_memblock.o		= -O3 -fomit-frame-pointer
CFLAGS_report.o			= -O3 -fomit-frame-pointer
CFLAGS_shadow.o			= -O3 -fomit-frame-pointer
CFLAGS_spinlock.o               = -O3
CFLAGS_stack.o			= -O3 -fomit-frame-pointer
CFLAGS_stack_depot.o		= -O3 -fomit-frame-pointer
CFLAGS_stat.o			= -O3 -fomit-frame-pointer
CFLAGS_supp.o			= -O3 -fomit-frame-pointer
CFLAGS_sync_atomic.o		= -O3 -fomit-frame-pointer
CFLAGS_sync_atomic_no_ktsan.o	= -O3 -fomit-frame-pointer
CFLAGS_sync_mtx.o		= -O3 -fomit-frame-pointer
CFLAGS_sync_percpu.o		= -O3 -fomit-frame-pointer
CFLAGS_sync_seqlock.o		= -O3 -fomit-frame-pointer
CFLAGS_sync.o			= -O3 -fomit-frame-pointer
CFLAGS_tab.o			= -O3 -fomit-frame-pointer
CFLAGS_tests_inst.o		= -O3 -fomit-frame-pointer
CFLAGS_tests_noinst.o		= -O3 -fomit-frame-pointer
CFLAGS_tests.o			= -O3 -fomit-frame-pointer
CFLAGS_thr.o			= -O3 -fomit-frame-pointer
CFLAGS_trace.o			= -O3 -fomit-frame-pointer

KTSAN_SANITIZE := n
KTSAN_SANITIZE_tests_inst.o = y
