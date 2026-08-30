#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(){
    printf("pid=%d\n", getpid());
    // 200KB，大于128KB阈值，glibc会调用mmap，不走brk heap
    void *p = malloc(200*1024);
    printf("malloc ptr = %p\n", p);

    // 阻塞，方便另一个终端 cat /proc/<pid>/maps
    getchar();
    free(p);
    getchar();
    return 0;
}
