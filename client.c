#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define FIB_OUTPUT "fib_output"
#define FIB_TIME "fib_time"

#define UBN_STR_SIZE 233
#define FIB_MAX 1000

struct timespec get_diff(struct timespec start, struct timespec end)
{
    struct timespec diff;

    diff.tv_sec = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;

    return diff;
}

int main()
{
    char buf[UBN_STR_SIZE];
    int offset = 1000;
    long long kt_fib, kt_copy;
    FILE *fib_output_fp, *fib_time_fp;
    struct timespec start, end;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }


    fib_output_fp = fopen(FIB_OUTPUT, "w");
    fib_time_fp = fopen(FIB_TIME, "w");

    if (!fib_output_fp || !fib_time_fp) {
        perror("Failed to open file for writing");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        int ret = 0;

        lseek(fd, i, SEEK_SET);

        /* Time elapsed by read() */
        clock_gettime(CLOCK_MONOTONIC, &start);
        ret = read(fd, buf, UBN_STR_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (ret < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        struct timespec diff = get_diff(start, end);
        long long ut_fib = diff.tv_nsec + diff.tv_sec * 1000000;

        /* Time elapsed by fib_sequence() and ubn_to_str() */
        ret = read(fd, &kt_fib, sizeof(long long));
        if (ret < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        /* Time elapsed by copy_to_user() */
        ret = read(fd, &kt_copy, sizeof(long long));
        if (ret < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        fprintf(fib_output_fp,
                "Reading from " FIB_DEV
                " at offset %d, returned the sequence "
                "%s.\n",
                i, buf);
        fprintf(fib_time_fp, "%d\t%lld\t%lld\t%lld\n", i, ut_fib, kt_fib,
                kt_copy);
    }

    fclose(fib_time_fp);
    close(fd);
    return 0;
}
