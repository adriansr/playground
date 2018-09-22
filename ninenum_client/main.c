#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DIGITS 9
#define LINE_LEN (DIGITS + 1)
#define BUF_LINES 1024

volatile int terminated = 0;

void signal_catch(int ignore) {
    terminated = 1;
}

char buf[LINE_LEN * BUF_LINES];

void ninenum_to_buf(unsigned int num, char *b) {
    for (int digit = DIGITS-1; digit >= 0; digit --) {
        b[digit] = '0' + (num % 10);
        num /= 10;
    }
}

int do_write_buf(int len) {
    int written = 0;
    while (written < len) {
        ssize_t nw = write(1, &buf[written], len - written);
        written += nw;
        if (nw <= 0) {
            return nw;
        }
    }
    return len;
}

uint64_t gettime(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        fprintf(stderr, "gettimeofday failed (errno=%d)\n", errno);
        exit(3);
    }
    return (uint64_t)tv.tv_sec*1000000LL + tv.tv_usec;
}

int is_descriptor_valid(int fd) {
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

void sleep_nanos(int nanos) {
    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = nanos;
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <num> <terminate> <base> <increment>\n", argv[0]);
        return 1;
    }
    unsigned int num = atoi(argv[1]);
    int terminate = atoi(argv[2]);
    unsigned int base = atoi(argv[3]);
    unsigned int inc = atoi(argv[4]);

    // prepare buf
    int i;
    for (i=0; i<BUF_LINES; i++) {
        buf[i*LINE_LEN + DIGITS] = '\n';
    }

    uint64_t start = gettime();

    int sent = 0;
    unsigned int cur = base;
    while (sent < num) {
        int lim = sent + BUF_LINES;
        int pos = 0;
        if (lim > num) lim = num;
        while (sent < lim) {
            ninenum_to_buf(cur, &buf[pos]);
            cur += inc;
            sent ++;
            pos += LINE_LEN;
        }
        if (do_write_buf(pos) <= 0 ) {
            fprintf(stderr, "write failed (returned %d, errno=%d)\n", pos, errno);
            exit(2);
        }
    }

    uint64_t took_us = gettime() - start;

    if (terminate) {
        char msg[] = "terminate\n";
        memcpy(buf, msg, sizeof(msg));
        do_write_buf(sizeof(msg));
        signal(SIGTERM, signal_catch);
        uint64_t term_start = gettime();
        while (!terminated && is_descriptor_valid(1)) {
            sleep_nanos(100000);
        }
        uint64_t term_took = gettime() - term_start;
        fprintf(stderr, "Termination took %.03lf ms\n", (double)term_took/1000.0);
    }
    fprintf(stderr, "Delivery of %d numbers took %.03lf ms (%.03lf n/s)\n", sent, (double)took_us/1000.0, (double)sent*1000000.0 / (double)took_us);
    return 0;
}

