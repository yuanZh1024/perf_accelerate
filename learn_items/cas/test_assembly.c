#include <stdint.h>
uint64_t val;

void cas_spin()
{
    uint64_t old = val;
    while (!__sync_bool_compare_and_swap(&val, old, 0x1234)) {
        old = val;   // ▒~G~M▒~B▒▒~Y▒~@▒~L
    }
}

int main() {
        cas_spin(); 

}
