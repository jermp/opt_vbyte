#include <unistd.h>
#include <fcntl.h>

int main() {

    sync();

    // from command line, use: echo 3 | sudo tee /proc/sys/vm/drop_caches

    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (!fd) {
        return 1;
    }
    if (write(fd, "3\n", 2) != 2) { // 1 only flushes PageCache;
                                    // 3 also flushes inodes and dentries
        return 2;
    }

    close(fd);
    sync();

    return 0;
}
