#include <stdio.h>

int main()
{
    int value = 100;
    
    int ret = __sync_bool_compare_and_swap(
        &value,
        100,
        200
    );

    printf("result = %d\n", ret);
    printf("value  = %d\n", value);

    return 0;
}


