#include <raylib.h>
#include "ajolot.h"

unsigned int Entry = 1;
bool run = true;

void main_loop() {
    while(run) {
         call_entry(Entry);
    }
}