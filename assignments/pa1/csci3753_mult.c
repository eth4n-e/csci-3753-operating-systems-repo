#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h> // header for copy_to_user
#include <stddef.h>        // header for size_t

// defining the system call
SYSCALL_DEFINE3(csci3753_mult, int, number1, int, number2, long *, result) {
  printk(KERN_ALERT, "Number 1: %d\n", number1);
  printk(KERN_ALERT, "Number 2: %d\n", number2);

  long kernel_result;
  kernel_result = number1 * number2;
  size_t res_len = sizeof(kernel_result);
  printk(KERN_ALERT, "Result of multiplication: %ld\n", kernel_result);
  // copy_to_user returns 0 on success, num_bytes not copied on failure
  // copy result to user space from kernel space
  if (copy_to_user(result, &kernel_result, res_len)) {
    printk(KERN_ALERT,
           "Unable to copy data from kernel space to user space.\n");
  }

  return 0;
}
