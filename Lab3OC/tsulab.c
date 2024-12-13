#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>

static int __init tsulab_init(void) {
    pr_info("Welcome to the Tomsk State University\n");
    return 0;
}

static void __exit tsulab_exit(void) {
    pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");