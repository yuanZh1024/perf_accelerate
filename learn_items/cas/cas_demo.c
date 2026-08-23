#define _GNU_SOURCE

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#define THREADS 32
#define ITERATIONS 1000000

// volatile val 告诉编译器：每次读这个变量，必须生成访存指令，不能缓存到寄存器
volatile int counter = 0;

void *worker(void *arg)
{
    for (int i = 0; i < ITERATIONS; i++) {
        int old = counter;
        while(!__sync_bool_compare_and_swap(
                    &counter,
                    old,
                    old + 1))  {
              old = counter;              
        }


        // while (1) {
  
        //     //取当前的值，这里注意“内存可见性问题” 所以用volatile保证读取counter 是从内存中直接读取的
        //     int old = counter;

        //     if (__sync_bool_compare_and_swap(
        //             &counter,
        //             old,
        //             old + 1)) {

        //         break;
        //     }
        // }
    }

    return NULL;
}

int main()
{
    pthread_t threads[THREADS];

    for (int i = 0; i < THREADS; i++) {
        pthread_create(
            &threads[i],
            NULL,
            worker,
            NULL
        );
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(
            threads[i],
            NULL
        );
    }

    printf("counter = %d\n", counter);

    return 0;
}

