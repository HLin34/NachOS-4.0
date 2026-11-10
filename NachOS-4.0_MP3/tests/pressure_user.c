#include "syscall.h"
volatile char payload[14000];
int main() {
    volatile int n;
    if (payload[0] != 0 || payload[13999] != 0) Exit(-77);
    payload[0] = 17; payload[13999] = 23;
    for (n = 0; n < 1000; ++n) {}
    PrintInt(7);
    for (n = 0; n < 5000; ++n) {}
    if (payload[0] == 17 && payload[13999] == 23 && payload[7000] == 0) Exit(77);
    Exit(-77);
}
