#include "ktsan.h"

void kt_event_disable(kt_thr_t *thr)
{
	thr->event_depth++;
}

void kt_event_enable(kt_thr_t *thr)
{
	thr->event_depth--;
	BUG_ON(thr->event_depth < 0);
}
