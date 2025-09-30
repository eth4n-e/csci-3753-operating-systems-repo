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
  size_t bytes_available;
  ssize_t bytes_to_copy;
  unsigned long uncopied_bytes;
  ssize_t total_bytes_copied;

  bytes_available = BUFFER_SIZE - (size_t)*offset;

  if (*offset >= BUFFER_SIZE) { // EOF, no bytes to copy
    bytes_available = 0;
  }

  if (length < bytes_available) {
    bytes_to_copy = length;
  } else { // attempt to read more than buffer
    printk(KERN_ALERT "%s: reading %ld bytes will overflow the buffer\n",
           DEVICE_NAME, length);
    bytes_to_copy = bytes_available;
  }

  // copy_to_user returns # bytes failed to copy to user
  // copying can fail for # of reasons -> typically issue with user space buf ->
  // no retry in kernel
  uncopied_bytes = copy_to_user(buffer, device_buffer, bytes_to_copy);
  total_bytes_copied = bytes_to_copy - uncopied_bytes;

  printk(KERN_ALERT "%s: %zd bytes read from device\n", DEVICE_NAME,
         total_bytes_copied);

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
  size_t bytes_available;
  ssize_t bytes_to_copy;
  unsigned long uncopied_bytes;
  ssize_t total_bytes_copied;

  bytes_available = BUFFER_SIZE - (size_t)*offset;

  if (*offset >= BUFFER_SIZE) { // EOF, no space to copy bytes
    bytes_available = 0;
  }

  if (length < bytes_available) {
    bytes_to_copy = length;
  } else { // write request would cause buffer overflow
    printk(KERN_ALERT "%s: writing %ld bytes will overflow the buffer\n",
           DEVICE_NAME, length);
    bytes_to_copy = bytes_available;
  }

  uncopied_bytes = copy_from_user(device_buffer, buffer, bytes_to_copy);
  total_bytes_copied = bytes_to_copy - uncopied_bytes;

  printk(KERN_ALERT "%s: %zd bytes written to device\n", DEVICE_NAME,
         total_bytes_copied);

  *offset += total_bytes_copied;

  return total_bytes_copied;
}

int pa3_char_driver_open(struct inode *pinode, struct file *pfile) {
  /* print to the log file that the device is opened and also print the number
   * of times this device has been opened until now*/
  printk(KERN_ALERT "%s: Device has now been opened %d times\n", DEVICE_NAME,
         ++open_count);
  return 0;
}

int pa3_char_driver_close(struct inode *pinode, struct file *pfile) {
  /* print to the log file that the device is closed and also print the number
   * of times this device has been closed until now*/
  printk(KERN_ALERT "%s: Device has now been closed %d times\n", DEVICE_NAME,
         ++close_count);
  return 0;
}

loff_t pa3_char_driver_seek(struct file *pfile, loff_t offset, int whence) {
  /* Update open file position according to the values of offset and whence */
  loff_t curr_off = pfile->f_pos;
  loff_t new_off;

  // custom behavior depending on whence
  switch (whence) {
  case 0: // start
    new_off = offset;
    break;
  case 1: // current
    new_off = offset + curr_off;
    break;
  case 2: // end
    new_off = offset + BUFFER_SIZE;
    break;
  default: // only considering whence 0-2
    return -EINVAL;
    break;
  }

  // bound checking for all seek strategies
  if (new_off < 0) { // prevent underflow
    new_off = 0;
  } else if (new_off > BUFFER_SIZE) { // prevent overflow
    new_off = BUFFER_SIZE;
  }

  pfile->f_pos = new_off;
  printk(KERN_ALERT "%s: device now at position %lld\n", DEVICE_NAME, new_off);
  // seek returns new_off on success -> see:
  // https://man7.org/linux/man-pages/man2/lseek.2.html
  return new_off;
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
  printk(KERN_ALERT "%s: inside %s function\n", DEVICE_NAME, __FUNCTION__);
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
  printk(KERN_ALERT "%s: inside %s function\n", DEVICE_NAME, __FUNCTION__);
  unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
  kfree(device_buffer);
  // source:
  // https://stackoverflow.com/questions/1879550/should-one-really-set-pointers-to-null-after-freeing-them
  device_buffer = NULL;
}

/* add module_init and module_exit to point to the corresponding init and exit
 * function*/
module_init(pa3_char_driver_init);
module_exit(pa3_char_driver_exit);
