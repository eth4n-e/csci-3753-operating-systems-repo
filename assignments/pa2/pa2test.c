#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h> // for O_RDWR
#include <errno.h> // for errno
#include <string.h>
// Macros
#define READ_WRITE_MODE "r+"
#define READ_BUF_SIZE 4096 
#define INPUT_BUF_SIZE 80

char* program;

void handle_seek(int fd, off_t offset, int whence) {
    off_t res_offset = lseek(fd, offset, whence);

    if (res_offset == -1) {
        printf("%s: error seeking through file\n", program);
        exit(EXIT_FAILURE);
    }

    return;
}

void handle_null_terminate(char* buffer, size_t* len) {
    *len = strlen(buffer);
    
    if (*len > 0 && buffer[(*len) - 1] == '\n') {
        buffer[(*len) - 1] = '\0';
    } else {
        // Edge case: user enters string > buf_size
        // fgets would read buf_size - 1 chars and add \0 as last char
        // strlen would return buf_size - 1 chars (leaving out \0)
        // add 1 to ensure we capture the null terminator
        *len += 1;
    }
}

void handle_write(int fd, char* buffer, ssize_t bytes_requested) {
    ssize_t total_bytes_written = 0;
    char* bufw = buffer;

    while(total_bytes_written < bytes_requested) {
        int bytes_to_write = bytes_requested - total_bytes_written;
        ssize_t num_written = write(fd, bufw, bytes_to_write);

        if (num_written == -1) {
            printf("%s: error writing to file\n");
            exit(EXIT_FAILURE);
        }

        total_bytes_written += num_written;
        bufw += num_written;
    }

    return;
}

ssize_t handle_read(int fd, char* buffer, ssize_t bytes_requested) {
    ssize_t total_bytes_read = 0;
    // local pointer to walk through buffer & prevent mem leaks
    char* bufr = buffer;

    while(total_bytes_read < bytes_requested) {
        int bytes_to_read = bytes_requested - total_bytes_read; 
        ssize_t num_read = read(fd, bufr, bytes_to_read);
        if (num_read == 0) { // EOF
            break;
        } else if (num_read == -1) { // file reading error
            printf("%s: error reading file\n", program);
            exit(EXIT_FAILURE); 
        }

        total_bytes_read += num_read;
        bufr += num_read;
    }

    return total_bytes_read;
}

void handle_input(char* buffer, int num_char, FILE *stream) {
    if (fgets(buffer, num_char, stream) == NULL) {
        exit(EXIT_FAILURE);
    }
}

long handle_numeric_input(char* buffer, int num_char, FILE *stream) {
    handle_input(buffer, num_char, stream);

    char* endptr;
    long value;
    value = strtol(buffer, &endptr, 10);
    if (endptr == buffer) { // unable to parse to long
        return 0; 
    }

    return value;
}

const char* prompt = "Option (r for read, w for write, s for seek): ";

int main(int argc, char** argv[]) {
    program = argv[0];
    if (argc < 2) {
        printf("%s error: missing filename\n", program);
        // EXIT_FAILURE represents a non-zero exit code but is more portable to non-UNIX environments
        exit(EXIT_FAILURE);
    } else if (argc > 2) {
        printf("%s error: too many parameters\n", program);
        exit(EXIT_FAILURE);
    }
    
    // TODO: ensure that we block for control d
    // setup_signal_handlers();
    const char* path = argv[1];
    // read + write to file, no mode bc we will not create file
    int flags = O_RDWR;
    int fd = open(path, flags);

    if (fd == -1) {
       printf("%s error: invalid filename\n", program); 
       exit(EXIT_FAILURE);
    }

    // buffers for file processing
    char* read_buf = (char *) malloc(READ_BUF_SIZE);  
    char* input_buf = (char *) malloc(INPUT_BUF_SIZE);
    if (read_buf == NULL || input_buf == NULL) {
        printf("%s: error allocating buffer\n", program);
        exit(EXIT_FAILURE);
    }

    long bytes_to_read;
    long offset;
    long whence;

    // process user input and interact with file
    while (1) {
        printf("%s", prompt);
        if(fgets(input_buf, INPUT_BUF_SIZE, stdin) == NULL) {
            printf("%s: error reading input\n", program);
            exit(EXIT_FAILURE);
        }
        
        // only accept the first character as the option
        int option = input_buf[0]; 
        switch(option) {
            case 'r':
                printf("Enter the number of bytes you want to read: ");

                bytes_to_read = handle_numeric_input(input_buf, INPUT_BUF_SIZE, stdin); 

                ssize_t num_read = handle_read(fd, read_buf, bytes_to_read);
                // only print number of bytes read into buffer 
                printf("%.*s\n", (int) num_read, read_buf);
                break;
            case 'w':
                printf("Enter the data you want to write: ");
                handle_input(input_buf, INPUT_BUF_SIZE, stdin);

                size_t len;
                handle_null_terminate(input_buf, &len);
                
                handle_write(fd, input_buf, len);
                break;
            case 's':
                printf("Enter an offset value: ");
                offset = handle_numeric_input(input_buf, INPUT_BUF_SIZE, stdin);      

                printf("Enter a value for whence: ");
                whence = handle_numeric_input(input_buf, INPUT_BUF_SIZE, stdin);
                if (whence < 0 || whence > 3) break;
                
                handle_seek(fd, offset, whence);
                break;
            default:
                // do nothing in this instance
                break;
        }
        
    }

    // cleanup
    free(read_buf);
    free(input_buf);
    close(fd);
}

