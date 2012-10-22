#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

static int __init mydrm_init(void)
{
	return 0;
}

static void __exit mydrm_exit(void)
{

}

module_init(mydrm_init);
module_exit(mydrm_exit);
