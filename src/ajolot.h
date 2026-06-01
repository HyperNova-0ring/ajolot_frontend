#define WIN_X 640
#define WIN_Y 480
#define WIN_HZ 60
#define WIN_NAME "Ajolot"

#include <raylib.h>

#define MAX_SVG_SHAPES 2048

typedef enum
{
	SHP_PATH,
	SHP_CIRCLE,
	SHP_RECT,
	SHP_ELLIPSE,
	SHP_LINE,
	SHP_POLYLINE,
	SHP_POLYGON
} ShapeKind;

typedef enum { CAP_BUTT, CAP_ROUND, CAP_SQUARE } StrokeCap;
typedef enum { JOIN_MITER, JOIN_ROUND, JOIN_BEVEL } StrokeJoin;

typedef struct
{
	ShapeKind kind;
	Color fill;
	Color stroke;
	float strokeWidth;
	bool hasFill, hasStroke;
	StrokeCap lineCap;
	StrokeJoin lineJoin;
	Vector2 *pts;
	int nPts;
	bool closed;
} SvgShape;

typedef struct
{
	SvgShape shapes[MAX_SVG_SHAPES];
	int nShapes;
	float vw, vh; // original viewBox dims
} SvgDoc;

typedef struct
{
	float jitter; // displacement scale 0–25
	float roughness; // turbulence base freq 0.01–0.2
	int fps; // animation FPS 1-24
	float strokeMult; // stroke width multiplier
	float colorHue; // global hue shift 0–360
	float alpha;    // opacity 0.0–1.0
	float seed;
	float timer;
} CrayonState;

bool svg_load(const char *path, SvgDoc *doc);
bool svg_load_from_memory(const unsigned char *data, int size, SvgDoc *doc);
void svg_free(SvgDoc *doc);
void draw_svg(const SvgDoc *doc, float screenX, float screenY,
	float drawW, float drawH, const CrayonState *cr, float strokeMult);

// EXTERN VAR.
extern bool run;
extern unsigned int Entry;
extern Music void_music;
// INTRO FRAMES
extern SvgDoc logo_doc;
// MENU DATA
extern SvgDoc games_doc;
extern SvgDoc apps_doc;
extern SvgDoc settings_doc;
extern SvgDoc power_doc;

// MUSIC FUNCTS.
void music_init(const char *path);
void music_init_from_track(Music track);
void music_cleanup(void);
void music_start(void);
void music_stop(void);
void music_fade(float target_vol, float duration_sec);

// INIT FUNCTS.
void init_frontend(void);
void load_data(void);

// ENTRY FUNCTS
void call_entry(unsigned int entry);

// MAIN LOOP FUNCTS.
void main_loop(void);

// END FUNCTS
void end_frontend();
void unload_data();

// MISC FUNCTS.
void end_frontend(void);
