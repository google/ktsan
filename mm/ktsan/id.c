#include "ktsan.h"

#include <linux/spinlock.h>

void kt_id_init(kt_id_manager_t *mgr)
{
	int i;

	for (i = 0; i < KT_MAX_THREAD_ID - 1; i++)
		mgr->ids[i] = i + 1;
	mgr->ids[KT_MAX_THREAD_ID - 1] = -1;
	mgr->head = 0;
	spin_lock_init(&mgr->lock);
}

int kt_id_new(kt_id_manager_t *mgr, void* data)
{
	int id;

	spin_lock(&mgr->lock);
	if (mgr->head == -1)
		return -1;
	id = mgr->head;
	mgr->head = mgr->ids[mgr->head];
	mgr->data[id] = data;
	spin_unlock(&mgr->lock);

	return id;
}

void kt_id_free(kt_id_manager_t *mgr, int id)
{
	spin_lock(&mgr->lock);
	mgr->ids[id] = mgr->head;
	mgr->head = id;
	spin_unlock(&mgr->lock);
}

void* kt_id_get_data(kt_id_manager_t *mgr, int id)
{
	void *data;

	spin_lock(&mgr->lock);
	data = mgr->data[id];
	spin_unlock(&mgr->lock);

	return data;
}
