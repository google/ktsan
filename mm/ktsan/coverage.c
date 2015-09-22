#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

void __fuzz_coverage(void)
{
	struct task_struct *t;
	unsigned long pos;

	t = current;
	if (!t || !t->ktsan.cover || in_interrupt())
		return;
	pos = t->ktsan.cover_pos++ & t->ktsan.cover_mask;
	t->ktsan.cover[pos] = (u32)_RET_IP_;
}
EXPORT_SYMBOL(__fuzz_coverage);

static ssize_t cover_write(struct file *file, const char __user *addr,
	size_t len, loff_t *pos)
{
	struct task_struct *t;
	unsigned long size;
	char buf[32];

	if (len >= sizeof(buf))
		return -E2BIG;
	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, addr, len))
		return -EFAULT;

	t = current;
	if (!strncmp(buf, "enable=", sizeof("enable=") - 1)) {
		if (t->ktsan.cover)
			return -EEXIST;
		if (kstrtoul(buf + sizeof("enable=") - 1, 10, &size))
			return -EINVAL;
		if (size <= 0 || size > (128<<20) || size & (size - 1))
			return -EINVAL;
		t->ktsan.cover = vmalloc(size * sizeof(u32));
		if (!t->ktsan.cover)
			return -ENOMEM;
		t->ktsan.cover_pos = 0;
		t->ktsan.cover_mask = size - 1;
	} else if (!strcmp(buf, "disable")) {
		if (!t->ktsan.cover)
			return -EEXIST;
		vfree(t->ktsan.cover);
		t->ktsan.cover = NULL;
		t->ktsan.cover_pos = 0;
		t->ktsan.cover_mask = 0;
	} else if (!strcmp(buf, "reset")) {
		if (!t->ktsan.cover)
			return -ENOTTY;
		t->ktsan.cover_pos = 0;
	} else
		return -ENXIO;

	return len;
}

static ssize_t cover_read(struct file *file, char __user *addr, size_t len,
	loff_t *pos)
{
	struct task_struct *t;

	t = current;
	if (!t->ktsan.cover)
		return -ENODEV;
	len = min(len, t->ktsan.cover_pos * sizeof(u32));
	len = min(len, (t->ktsan.cover_mask + 1) * sizeof(u32));
	if (copy_to_user(addr, t->ktsan.cover, len))
		return -EFAULT;
	return len;
}

static const struct file_operations cover_ops = {
	.open		= simple_open,
	.llseek		= noop_llseek,
	.read		= cover_read,
	.write		= cover_write,
};

static __init int coverage_init(void)
{
	proc_create("cover", S_IRUSR, NULL, &cover_ops);
	return 0;
}

device_initcall(coverage_init);
