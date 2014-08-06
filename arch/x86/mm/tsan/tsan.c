#include <linux/tsan.h>

#include <linux/printk.h>

void tsan_thread_start(int thread_id, int cpu) {
	static int i = 0;
	if (++i < 100)
		pr_err("TSan: Thread #%d started on cpu #%d.\n",
		       thread_id, cpu);
}
