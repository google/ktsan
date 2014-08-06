/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_TSAN_H
#define LINUX_TSAN_H

#ifdef CONFIG_TSAN

void tsan_thread_start(int thread_id, int cpu);

#else /* CONFIG_TSAN */

/* When disabled TSAN is no-op. */

void tsan_thread_start(int thread_id, int cpu) {}

#endif /* CONFIG_TSAN */

#endif /* LINUX_TSAN_H */
