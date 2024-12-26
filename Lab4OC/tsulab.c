#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

#define PROC_FILE_NAME "tsulab"

static struct proc_dir_entry *proc_file;

static ssize_t proc_file_read(struct file *file, char __user *buffer, size_t count, loff_t *offset) {
    struct timespec64 ts;
    struct tm tm;
    int len = 0;
    char msg[64];
    int hours, minutes, seconds;
    int total_seconds, elapsed_seconds;
    int percentage;

    ktime_get_real_ts64(&ts);
    time64_to_tm(ts.tv_sec, 0, &tm);

    hours = tm.tm_hour;
    minutes = tm.tm_min;
    seconds = tm.tm_sec;

    total_seconds = 24 * 60 * 60; // Total seconds in a day
    elapsed_seconds = hours * 60 * 60 + minutes * 60 + seconds;
    percentage = ((total_seconds - elapsed_seconds) * 100) / total_seconds;

    len = sprintf(msg, "Percentage of the day remaining: %d%%\n", percentage);

    if (*offset > 0) {
        return 0;
    }

    if (copy_to_user(buffer, msg, len)) {
        return -EFAULT;
    }

    *offset = len;
    return len;
}

static const struct proc_ops proc_file_ops = {
    .proc_read = proc_file_read,
};

static int __init tsulab_init(void) {
    pr_info("Welcome to the Tomsk State University\n");

    proc_file = proc_create(PROC_FILE_NAME, 0444, NULL, &proc_file_ops);
    if (!proc_file) {
        pr_err("Failed to create /proc/%s\n", PROC_FILE_NAME);
        return -ENOMEM;
    }

    return 0;
}

static void __exit tsulab_exit(void) {
    pr_info("Tomsk State University forever!\n");

    if (proc_file) {
        proc_remove(proc_file);
    }
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");
