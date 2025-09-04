#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(helloworld) {
  printk(KERN_ALERT "hello world\n");
  return 0;
}
