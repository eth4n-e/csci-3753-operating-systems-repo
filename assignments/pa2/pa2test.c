#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h> // for O_RDWR
#include <errno.h> // for errno
// Macros
#define READ_WRITE_MODE "r+"
#define READ_BUF_SIZE 65536
#define WRITE_BUF_SIZE 4096
#define MIN(a,b) ((a) < (b) ? (a) : (b)) // "min function in c" in google

char* program;

ssize_t handle_read(int fd, char* buffer, ssize_t bytes_requested) {
    ssize_t total_bytes_read = 0;
    // local pointer to walk through buffer & prevent mem leaks
    char* bufr = buffer;

    while(total_bytes_read < bytes_requested) {
        // avoid buffer overflow by always reading <= buf size bytes
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

void read_byte_input(int* input_store) {
    int result = scanf(" %d", input_store);

    if (result == 0) return;
    if (result == -1) {
        printf("Read error or EOF received, exiting process.\n");
        exit(EXIT_FAILURE);
    }

    return;
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

    // variables for file processing
    ssize_t file_read; 
    char* bufr = (char *) malloc(READ_BUF_SIZE);  
    char* bufw = (char *) malloc(WRITE_BUF_SIZE);
    if (bufr == NULL || bufw == NULL) {
        printf("%s: error allocating buffer\n", program);
        exit(EXIT_FAILURE);
    }

    int bytes_requested;
    int byte_cursor;

    // process user input and interact with file
    while (1) {
        printf("%s", prompt);
        int option = getchar();

        switch(option) {
            case 'r':
                printf("Enter the number of bytes you want to read: ");

                read_byte_input(&bytes_requested);                
                // TODO: do I have to make this +=?
                byte_cursor = handle_read(fd, bufr, bytes_requested);
                // only print number of bytes read into buffer 
                printf("%.*s\n", (int) byte_cursor, bufr);
                break;
            case 'w':
                // handle_write();
                printf("Enter the data you want to write: ");
                // if(fgets(bufw, WRITE_BUF_SIZE, stdin) == NULL) {
 //                    printf("%s: error reading user input or EOF reached.\n", program);
               //  }
                handle_read(STDIN_FILENO, bufw, WRITE_BUF_SIZE);
                printf("%s", bufw);
                break;
            case 's':
                // handle_seek();
                printf("Enter an offset value: \n");
                printf("Enter a value for whence: ");
                break;
            default:
                // do nothing in this instance
                break;
        }

        // Source: internet search "getchar ignore newline"
        // Noticed initially that my prompt was printing multiple times 
        // because newline character was being read on subsequent iteration 
        // placed at end of loop bc provided executable re-prints prompt with leading whitespace
        while((option = getchar()) != '\n' && option != EOF);
    }

    // cleanup
    free(bufr);
    free(bufw);
    close(fd);
}

