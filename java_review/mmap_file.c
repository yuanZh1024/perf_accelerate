#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define SIZE 4096

void pause_here(const char *msg)
{
    printf("\n========================================\n");
    printf("%s\n", msg);
    printf("PID = %d\n", getpid());
    printf("Press ENTER to continue...\n");
    printf("========================================\n");

    getchar();
}

int main()
{
    const char *filename = "test.dat";

    /*
     * 1. 创建文件
     */
    int fd = open(
        filename,
        O_RDWR | O_CREAT | O_TRUNC,
        0644
    );

    if (fd < 0) {
        perror("open");
        exit(1);
    }

    /*
     * 文件大小 = 4096
     */
    if (ftruncate(fd, SIZE) < 0) {
        perror("ftruncate");
        exit(1);
    }

    /*
     * 写入初始内容
     */
    const char *initial = "AAAAAAAAAAAAAAAA";

    if (pwrite(fd, initial, strlen(initial), 0) < 0) {
        perror("pwrite");
        exit(1);
    }

    printf("Initial file content: %s\n", initial);

    pause_here("1. Before mmap");


    /*
     * 2. MAP_PRIVATE
     */
    char *p = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE,
        fd,
        0
    );

    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    printf("mmap returned address: %p\n", p);

    pause_here("2. After MAP_PRIVATE mmap");


    /*
     * 3. 读取 mmap 内容
     */
    printf("Mapped memory before modification: %.16s\n", p);

    pause_here("3. Before modifying mapped memory");


    /*
     * 4. 修改 mmap 内存
     */
    p[0] = 'B';

    printf("Mapped memory after modification: %.16s\n", p);

    pause_here("4. After modifying mapped memory");


    /*
     * 5. 检查文件
     */
    printf("\nNow check test.dat from another terminal.\n");

    pause_here("5. Check whether file changed");


    /*
     * 6. 保持进程
     */
    printf("Process will keep running...\n");

    while (1) {
        sleep(10);
    }

    return 0;
}