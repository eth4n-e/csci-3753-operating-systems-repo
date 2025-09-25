#include <linux/init.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Ethan Epperson");
MODULE_LICENSE("GPL");

/* Define device_buffer and other global data structures you will need here */
#define DEVICE_NAME "pa3_char_driver_device"
#define BUFFER_SIZE 900
#define MAJOR_NUM 511

/* NTS:
 * Static vars retain value across function executions
 * within a single program run
 * in context of kernel:
 * - static vars initialized when module loads
 * - retain value as long as module remains loaded in kernel
 * - ^ result of the kernel being a continuously running proc
 * - ^ important for vars that need to persist throughout module usage
 */
static int open_count = 0;
static int close_count = 0;
static char *device_buffer;

ssize_t pa3_char_driver_read(struct file *pfile, char __user *buffer,
                             size_t length, loff_t *offset) {
  /* *buffer is the userspace buffer to where you are writing the data you want
   * to be read from the device file*/
  /* length is the length of the userspace buffer (more like size of requested
   * read)*/
  /* offset will be set to current position of the opened file after read*/
  /* copy_to_user function: source is device_buffer and destination is the
   * userspace buffer *buffer */
  ssize_t total_bytes_copied;
  ssize_t bytes_to_copy;

  if (length + *offset > BUFFER_SIZE) {
    printk("reading %d bytes will overflow the buffer\n", length + *offset);
    bytes_to_copy = BUFFER_SIZE - *offset;
  } else {
    bytes_to_copy = length;
  }

  // copy_to_user returns # bytes failed to copy to user
  // copying can fail for # of reasons -> typically issue with user space buf ->
  // no retry in kernel
  unsigned long uncopied_bytes =
      copy_to_user(buffer, device_buffer, bytes_to_copy);
  total_bytes_copied = bytes_to_copy - uncopied_bytes;

  printk(KERN_ALERT "%zd bytes read from device\n", total_bytes_copied);

  *offset += total_bytes_copied;

  // reads return # bytes read
  return total_bytes_copied;
}

ssize_t pa3_char_driver_write(struct file *pfile, const char __user *buffer,
                              size_t length, loff_t *offset) {
  /* *buffer is the userspace buffer where you are writing the data you want to
   * be written in the device file*/
  /* length is the length of the userspace buffer*/
  /* current position of the opened file*/
  /* copy_from_user function: destination is device_buffer and source is the
   * userspace buffer *buffer */

  return length;
}

int pa3_char_driver_open(struct inode *pinode, struct file *pfile) {
  /* print to the log file that the device is opened and also print the number
   * of times this device has been opened until now*/
  printk(KERN_ALERT "Device opened. Open count: %d\n", ++open_count);
  return 0;
}

int pa3_char_driver_close(struct inode *pinode, struct file *pfile) {
  /* print to the log file that the device is closed and also print the number
   * of times this device has been closed until now*/
  printk(KERN_ALERT "Device closed. Close count: %d\n", ++close_count);
  return 0;
}

loff_t pa3_char_driver_seek(struct file *pfile, loff_t offset, int whence) {
  /* Update open file position according to the values of offset and whence */
  return 0;
}

/*
 * .member_name = value
 * called a designated initializer
 * allows for initialization of specific members of a struct by name
 * dot . operator used to access member name of struct
 * e.g. .open = pa_open sets the open function pointer to use my open
 * purpose: define driver functions needed to interact with device file
 */
struct file_operations pa3_char_driver_file_operations = {
    .owner = THIS_MODULE,
    /* add the function pointers to point to the corresponding file operations.
       look at the file fs.h in the linux souce code*/
    .open = pa3_char_driver_open,
    .release = pa3_char_driver_close,
    .read = pa3_char_driver_read,
    .write = pa3_char_driver_write,
    .llseek = pa3_char_driver_seek};

static int pa3_char_driver_init(void) {
  /* print to the log file that the init function is called.*/
  /* register the device */
  printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
  // TODO: do I need to check for errors here / with exit func
  device_buffer = (char *)kmalloc(BUFFER_SIZE, GFP_KERNEL);
  if (!device_buffer) {
    // kmalloc returns null on failure
    // only fails if unable to allocate contiguous mem of size
    return -ENOMEM;
  }
  // makes device visible to kernel + allows for user-space processes to
  // interact
  register_chrdev(MAJOR_NUM, DEVICE_NAME, &pa3_char_driver_file_operations);
  return 0;
}

static void pa3_char_driver_exit(void) {
  /* print to the log file that the exit function is called.*/
  /* unregister the device using the unregister_chrdev() function. */
  printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
  unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
  kfree(device_buffer);
}

/* add module_init and module_exit to point to the corresponding init and exit
 * function*/
module_init(pa3_char_driver_init);
module_exit(pa3_char_driver_exit);
