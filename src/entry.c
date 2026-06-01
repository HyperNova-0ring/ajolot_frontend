#include <raylib.h>
#include <rlgl.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ajolot.h"

// Invalid.

void ajolot_entry_invalid() {
    BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Invalid Entry!", 0, 0, 14, WHITE);
    EndDrawing();
}

// Intro & Menu defs.

Camera3D cam3d = {
    .position   = (Vector3){0.0f, 0.0f, 0.0f},
    .target     = (Vector3){0.0f, 0.0f, -1.0f},
    .up         = (Vector3){0.0f, 1.0f, 0.0f},
    .projection = CAMERA_PERSPECTIVE,
    .fovy       = 90.0f,
};

Vector3 sphere_pos = { 0.0f, 0.0f, 0.0f };
float sphere_radius = 5.0f;
float sphere_rot = 0.0f;

void sphere_logicanddraw(void) {
    sphere_rot += 0.16;
    if (sphere_rot >= 360.0f) sphere_rot -= 360.0f;
    rlDisableBackfaceCulling();
    rlPushMatrix();
        rlRotatef(sphere_rot, 0.0f, 1.0f, 0.0f);
        DrawSphereWires(sphere_pos, sphere_radius, 32, 32, BLACK);
    rlPopMatrix();
    rlEnableBackfaceCulling();
}

// Intro.

#define INTRO_DELTA 60
int intro_timer = 0;
int intro_cap = 0;
Color rec_fade = {0x00, 0x00, 0x00, 0xFF};
Color logo_fade = { 0xFF, 0xFF, 0xFF, 0x00};
CrayonState logo_crayon = { .jitter = 1.0f, .roughness = 0.02f, .fps = 8, .strokeMult = 0.8f, .alpha = 0.0f };
float logo_crayon_timer = 0.0f;
int logo_size[2] = { 480, 360 }; // 1=x. 2=y
int logo_pos[2] = { WIN_X / 2, WIN_Y / 2}; // 1=x. 2=y

void update_logoanddraw() {
    logo_crayon.alpha = logo_fade.a / 255.0f;
    logo_crayon_timer += GetFrameTime();
        if (logo_crayon_timer >= 1.0f / (float)logo_crayon.fps) {
            logo_crayon_timer = 0.0f;
            logo_crayon.seed = (float)GetRandomValue(0, 99999);
        }
        draw_svg(&logo_doc, logo_pos[0], logo_pos[1], logo_size[0], logo_size[1], &logo_crayon, logo_crayon.strokeMult);
}

void ajolot_entry_intro(void) {
    switch (intro_cap) { 
      case 0:
        intro_cap = 1;
      break;
      case 1: // appears screen from dark.
        BeginDrawing();
          ClearBackground(WHITE);
            BeginMode3D(cam3d);
                sphere_logicanddraw();
            EndMode3D();
          DrawRectangle(0, 0, WIN_X, WIN_Y, rec_fade);
        EndDrawing();
        if (rec_fade.a != 0x00 ) {
            rec_fade.a = rec_fade.a - 0x3;
        } else {
            if (intro_timer == 90) {
                intro_timer = 0;
                intro_cap = 2;
            } else {
                intro_timer++;
            }
        }
      break;
      case 2: // appears title
        BeginDrawing();
          ClearBackground(WHITE);
            BeginMode3D(cam3d);
                sphere_logicanddraw();
            EndMode3D();
            update_logoanddraw();
        EndDrawing();
        if (logo_fade.a != 0xFF ) {
            logo_fade.a = logo_fade.a + 0x5;
        } else {
            if (intro_timer == 180) {
                intro_timer = 0;
                intro_cap = 3;
            } else {
                intro_timer++;
            }
        }
      break;
      case 3: // vanish title
        BeginDrawing();
          ClearBackground(WHITE);
            BeginMode3D(cam3d);
                sphere_logicanddraw();
            EndMode3D();
            update_logoanddraw();
        EndDrawing();
        if (logo_fade.a != 0x00 ) {
            logo_fade.a = logo_fade.a - 0x5;
        } else {
            if (intro_timer == 180) {
                intro_timer = 0;
                intro_cap = 4;
            } else {
                intro_timer++;
            }
        }
      break;
      case 4: // next entry: menu.
        music_init_from_track(void_music);
        music_start();
        music_fade(0.15f, 3.0f);
        Entry = 2;
      break;
      default: // invalid cap.
        Entry = 0; //go to invalid entry.
        break;
    }
}

// Menu

#define MENU_DELTA 60
#define MENU_MIN_Y 360
#define MENU_MAX_Y 480
int menu_cap = 0;
int menu_timer = 0;
bool menu_show = true;
bool is_drawing = false;
int menu_pos_y = WIN_Y; // Outside of window view.
#define MENU_OFFSET 80
int menu_games_pos_x = (160 - MENU_OFFSET);
int menu_apps_pos_x = (320 - MENU_OFFSET);
int menu_settings_pos_x = (480 - MENU_OFFSET);
int menu_power_pos_x = (640 - MENU_OFFSET);
static CrayonState menu_crayon = { .jitter = 2.0f, .roughness = 0.03f, .fps = 8, .strokeMult = 1.0f, .alpha = 1.0f };
static float menu_crayon_timer = 0.0f;

void (*button_logic)() = NULL;

void update_menu_logic() {
//    TraceLog(LOG_INFO, "Button Func Enable");
}


void menu_logicanddraw() {
    if (menu_show) {
        if (menu_pos_y > MENU_MIN_Y) {
            if (menu_pos_y < MENU_MAX_Y && !is_drawing) is_drawing = true;
            menu_pos_y = menu_pos_y - 5;
            if (menu_pos_y <= MENU_MIN_Y) {
                menu_pos_y = MENU_MIN_Y;
                button_logic = update_menu_logic;
            }
        }
    } else {
        if (button_logic != NULL) button_logic = NULL;
        if (menu_pos_y < MENU_MAX_Y) {
            menu_pos_y = menu_pos_y + 5;
            if (menu_pos_y >= MENU_MAX_Y) {
                menu_pos_y = MENU_MAX_Y;
                is_drawing = false;
            }
        }
    }
    if (is_drawing) {
        menu_crayon_timer += GetFrameTime();
        if (menu_crayon_timer >= 1.0f / (float)menu_crayon.fps) {
            menu_crayon_timer = 0.0f;
            menu_crayon.seed = (float)GetRandomValue(0, 99999);
        }
        if (button_logic != NULL) button_logic();
        draw_svg(&games_doc,    menu_games_pos_x,    menu_pos_y + 50, 100, 80, &menu_crayon, menu_crayon.strokeMult);
        draw_svg(&apps_doc,     menu_apps_pos_x,     menu_pos_y + 50, 100, 80, &menu_crayon, menu_crayon.strokeMult);
        draw_svg(&settings_doc, menu_settings_pos_x, menu_pos_y + 50, 100, 80, &menu_crayon, menu_crayon.strokeMult);
        draw_svg(&power_doc,    menu_power_pos_x,    menu_pos_y + 50, 100, 80, &menu_crayon, menu_crayon.strokeMult);
    }
}

void ajolot_entry_menu(void) {
    switch (menu_cap) {
      case 0:
        BeginDrawing();
        ClearBackground(WHITE);
            BeginMode3D(cam3d);
                sphere_logicanddraw();
            EndMode3D();
            menu_logicanddraw();
        EndDrawing();
      break;
      default:
        Entry = 0;
      break;
    }
}

void ajolot_entry_load(void) {
}

// Entry struct.

typedef struct {
    void (*entry_invalid)();
    void (*entry_intro)();
    void (*entry_menu)();
    void (*entry_load)();
} EntryPtr;

const EntryPtr eptr = {
    .entry_invalid = ajolot_entry_invalid,
    .entry_intro = ajolot_entry_intro,
    .entry_menu = ajolot_entry_menu,
    .entry_load = ajolot_entry_load
};

// Call Entry.

void call_entry(unsigned int entry) {
    void (**fns)(void) = (void (**)(void)) &eptr;
    unsigned int count = sizeof(eptr) / sizeof(void (*)(void));
    fns[entry < count ? entry : 0]();
}
