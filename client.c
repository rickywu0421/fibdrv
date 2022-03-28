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
#define KTIME_DIR "ktime"
#define KTIME_FIB_KERN "fib_kern"
#define KTIME_FIB_USER "fib_user"
#define KTIME_FIB_COPY "fib_copy"

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
    // ubn_t fib;
    // char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */
    long long kt_fib, kt_copy;
    FILE *fib_output_fp, *fib_kern_fp, *fib_user_fp, *fib_copy_fp;
    struct timespec start, end;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }


    fib_output_fp = fopen(FIB_OUTPUT, "w");
    fib_kern_fp = fopen(KTIME_DIR "/" KTIME_FIB_KERN, "w");
    fib_user_fp = fopen(KTIME_DIR "/" KTIME_FIB_USER, "w");
    fib_copy_fp = fopen(KTIME_DIR "/" KTIME_FIB_COPY, "w");

    if (!fib_kern_fp || !fib_user_fp || !fib_copy_fp) {
        perror("Failed to open file for writing");
        exit(1);
    }

    /*for (int i = 0; i <= offset; i++) {
        long long sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }*/

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


        fprintf(fib_output_fp,
                "Reading from " FIB_DEV
                " at offset %d, returned the sequence "
                "%s.\n",
                i, buf);

        struct timespec diff = get_diff(start, end);
        long long ut_fib = diff.tv_nsec + diff.tv_sec * 1000000;

        fprintf(fib_user_fp, "%d\t%lld\n", i, ut_fib);

        /* Time elapsed by fib_sequence() and ubn_to_str() */
        ret = read(fd, &kt_fib, sizeof(long long));
        if (ret < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        fprintf(fib_kern_fp, "%d\t%lld\n", i, kt_fib);

        /* Time elapsed by copy_to_user() */
        ret = read(fd, &kt_copy, sizeof(long long));
        if (ret < 0) {
            perror("Failed to read from character device");
            exit(1);
        }

        fprintf(fib_copy_fp, "%d\t%lld\n", i, kt_copy);
    }


    /*(for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }*/

    fclose(fib_output_fp);
    fclose(fib_kern_fp);
    fclose(fib_user_fp);
    fclose(fib_copy_fp);
    close(fd);
    return 0;
}
