#include "syscall.h"
volatile char payload[14000];
int main() {
    if (payload[0] != 0 || payload[13999] != 0) Exit(-78);
    payload[0] = 19; payload[13999] = 29;
    if (payload[0] == 19 && payload[13999] == 29 && payload[7000] == 0) Exit(78);
    Exit(-78);
}
