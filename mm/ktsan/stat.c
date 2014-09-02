#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

static struct {
	kt_stat_t	i;
	const char	*s;
} desc[] = {
	{kt_stat_access_read,		"kt_stat_access_read"},
	{kt_stat_access_write,		"kt_stat_access_write"},
	{kt_stat_sync_objects,		"kt_stat_sync_objects"},
	{kt_stat_sync_alloc,		"kt_stat_sync_alloc"},
	{kt_stat_sync_free,		"kt_stat_sync_free"},
	{kt_stat_memblock_objects,	"kt_stat_memblock_objects"},
	{kt_stat_memblock_alloc,	"kt_stat_memblock_alloc"},
	{kt_stat_memblock_free,		"kt_stat_memblock_free"},
};

void kt_stat_collect(kt_stats_t *stat)
{
	kt_stats_t *stat1;
	int cpu, i;

	memset(stat, 0, sizeof(*stat));
	for_each_possible_cpu(cpu) {
		stat1 = &per_cpu_ptr(kt_ctx.cpus, cpu)->stat;
		for (i = 0; i < kt_stat_count; i++)
			stat->stat[i] += stat1->stat[i];
	}
}

static int kt_stat_show(struct seq_file *m, void *v)
{
	kt_stats_t stat;
	int i;

	kt_stat_collect(&stat);
	for (i = 0; i < ARRAY_SIZE(desc); i++)
		seq_printf(m, "%s: %lu\n", desc[i].s, stat.stat[desc[i].i]);
	return 0;
}

static int kt_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, kt_stat_show, NULL);
}

static const struct file_operations kt_stat_ops = {
	.open		= kt_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

void kt_stat_init(void)
{
	proc_create("ktsan_stats", S_IRUSR, NULL, &kt_stat_ops);
}
