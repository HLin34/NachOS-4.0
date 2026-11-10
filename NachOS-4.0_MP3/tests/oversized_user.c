#include "syscall.h"
volatile char payload[20000];
int main() { payload[0] = 1; payload[19999] = 2; Exit(-79); }
