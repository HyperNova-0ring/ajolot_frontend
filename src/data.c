#include <raylib.h>
#include <stdlib.h>
#include "ajolot.h"
#include "data/logo.h"
#include "data/games.h"
#include "data/apps.h"
#include "data/settings.h"
#include "data/power.h"
#include "data/void.h"

// Intro
SvgDoc logo_doc;
// Menu
SvgDoc games_doc;
SvgDoc apps_doc;
SvgDoc settings_doc;
SvgDoc power_doc;
// Music
Music void_music;

void load_data() {
    // Intro
    svg_load_from_memory(logo,	  sizeof(logo),	   &logo_doc);
    // Menu
    svg_load_from_memory(games,   sizeof(games),   &games_doc);
    svg_load_from_memory(apps,    sizeof(apps),    &apps_doc);
    svg_load_from_memory(setting, sizeof(setting), &settings_doc);
    svg_load_from_memory(power,   sizeof(power),   &power_doc);
    // Music
    void_music = LoadMusicStreamFromMemory(".ogg", void_ogg, void_ogg_len);
}

void unload_data() {
    // Intro
    svg_free(&logo_doc);
    // Menu
    svg_free(&games_doc);
    svg_free(&apps_doc);
    svg_free(&settings_doc);
    svg_free(&power_doc);
    // Music
    UnloadMusicStream(void_music);
}
