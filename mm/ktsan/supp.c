#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/kallsyms.h>

#ifndef CONFIG_KALLSYMS
#error "KTSAN suppressions require CONFIG_KALLSYMS"
#endif

struct kt_supp_s {
	const char *func;
	unsigned long start;
	unsigned long end;
	unsigned hits;
};
typedef struct kt_supp_s kt_supp_t;

static kt_supp_t suppressions[] = {
	{"generic_fillattr"},		/* Non-atomic read/write of timespec. https://lkml.org/lkml/2015/8/28/400 */
	{"atime_needs_update"},		/* Non-atomic read/write of timespec */
	{"generic_update_time"},	/* Non-atomic read/write of timespec */
	{"tcp_poll"},			/* Race on tp->rcv_nxt. Seems benign */
	{"vm_stat_account"},		/* Race on mm->total_vm. Stat accounting */
};

void kt_supp_init(void)
{
	int res, i;
	unsigned long pc, size, off;
	kt_supp_t *s;

	for (i = 0; i < ARRAY_SIZE(suppressions); i++) {
		s = &suppressions[i];
		pc = kallsyms_lookup_name(s->func);
		BUG_ON(pc == 0);
		size = off = 0;
		res = kallsyms_lookup_size_offset(pc, &size, &off);
		BUG_ON(!res || size == 0 || off != 0);
		s->start = pc;
		s->end = pc + size;
	}
}

bool kt_supp_suppressed(unsigned long pc)
{
	int i;
	kt_supp_t *s;

	for (i = 0; i < ARRAY_SIZE(suppressions); i++) {
		s = &suppressions[i];
		if (pc >= s->start && pc < s->end) {
			kt_atomic32_fetch_add_no_ktsan(&s->hits, 1);
			return true;
		}
	}
	return false;
}
