#include "ajolot.h"

int main(int argc, char *argv[]) {
    init_frontend();
    main_loop();
    end_frontend();
    return 0;
}