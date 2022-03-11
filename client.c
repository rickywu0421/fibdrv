#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    // char buf[1];
    uint64_t fib;
    // char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    /*for (int i = 0; i <= offset; i++) {
        long long sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }*/

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        if (read(fd, &fib, 1) < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lu.\n",
               i, fib);
    }

    /*(for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }*/

    close(fd);
    return 0;
}
