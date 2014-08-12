#include "ktsan.h"

#include <linux/kernel.h>

void ktsan_acquire(ktsan_thr_t *thr, uptr_t pc, uptr_t addr)
{
	ktsan_sync_t *sync;
	bool created;

	sync = ktsan_tab_access(&ktsan_ctx.synctab, addr, &created, false);
	if (created) {
	}
}

void ktsan_release(ktsan_thr_t *thr, uptr_t pc, uptr_t addr)
{
	ktsan_sync_t *sync;
	bool created;

	sync = ktsan_tab_access(&ktsan_ctx.synctab, addr, &created, false);
	if (created) {
	}
}
