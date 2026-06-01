#include <stdlib.h>
#include <raylib.h>
#include "ajolot.h"

void init_frontend() {
    InitWindow(WIN_X, WIN_Y, WIN_NAME);
    if (!IsWindowReady()) {
        TraceLog(LOG_FATAL, "Can't Initialize Window.");
        exit(1);
    }
//    ToggleFullscreen();
    InitAudioDevice();
    load_data();
    SetTargetFPS(60);
}

void end_frontend() {
    unload_data();
    CloseAudioDevice();
    CloseWindow();
}