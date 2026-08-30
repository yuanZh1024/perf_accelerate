#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define SIZE (4 * 4096)

void wait_for_observe(const char *msg)
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
    printf("PID = %d\n", getpid());

    /*
     * 1. anonymous + private
     */
    void *anon_private = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (anon_private == MAP_FAILED) {
        perror("mmap anon private");
        exit(1);
    } else {
        printf("mmap succeeded, returned address: %p\n", anon_private);
        // 可以故意访问一下防止优化
        *(char *)anon_private = 1;
    }

    wait_for_observe(
        "1. MAP_ANONYMOUS | MAP_PRIVATE"
    );


    /*
     * 2. anonymous + shared
     */
    void *anon_shared = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (anon_shared == MAP_FAILED) {
        perror("mmap anon shared");
        exit(1);
    } else {
        printf("mmap succeeded, returned address: %p\n", anon_shared);
        // 可以故意访问一下防止优化
        *(char *)anon_shared = 1;
    }

    wait_for_observe(
        "2. MAP_ANONYMOUS | MAP_SHARED"
    );


    /*
     * 创建测试文件
     */
    int fd = open(
        "mmap_test.dat",
        O_RDWR | O_CREAT | O_TRUNC,
        0644
    );

    if (fd < 0) {
        perror("open");
        exit(1);
    }

    /*
     * 文件扩展到 SIZE * 2
     */
    if (ftruncate(fd, SIZE * 2) < 0) {
        perror("ftruncate");
        exit(1);
    }


    /*
     * 3. file + private
     */
    void *file_private = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE,
        fd,
        0
    );

    if (file_private == MAP_FAILED) {
        perror("mmap file private");
        exit(1);
    } else {
        printf("mmap succeeded, returned address: %p\n", file_private);
        // 可以故意访问一下防止优化
        *(char *)file_private = 1;
    }

    wait_for_observe(
        "3. file + MAP_PRIVATE"
    );


    /*
     * 4. file + shared
     */
    void *file_shared = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (file_shared == MAP_FAILED) {
        perror("mmap file shared");
        exit(1);
    }else {
        printf("mmap succeeded, returned address: %p\n", file_shared);
        // 可以故意访问一下防止优化
        *(char *)file_shared = 1;
    }

    wait_for_observe(
        "4. file + MAP_SHARED"
    );


    /*
     * 5. same file, different offset
     */
    void *file_offset = mmap(
        NULL,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        SIZE
    );

    if (file_offset == MAP_FAILED) {
        perror("mmap file offset");
        exit(1);
    }else {
        printf("mmap succeeded, returned address: %p\n", file_offset);
        // 可以故意访问一下防止优化
        *(char *)file_offset = 1;
    }

    wait_for_observe(
        "5. same file + MAP_SHARED + offset=SIZE"
    );


    /*
     * 最后保持进程不退出
     */
    printf("\nAll mappings created.\n");
    printf("PID = %d\n", getpid());
    printf("Sleeping...\n");

    while (1) {
        sleep(10);
    }

    return 0;
}