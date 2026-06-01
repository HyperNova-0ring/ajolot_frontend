#include "ajolot.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_BILLBOARDS 64
#define PANEL_WIDTH 320
#define PANEL_PADDING 18
#define CRAYON_FPS_MAX 24

typedef struct
{
	Vector3 pos;
	float scale;
	SvgDoc doc;

	RenderTexture2D rt;
	Texture2D tex;
	bool textureReady;

	bool grabbed;
} Billboard;

typedef struct
{
	Camera3D cam;
	Billboard boards[MAX_BILLBOARDS];
	int nBoards;
	CrayonState crayon;
	bool panelOpen;
	int activeSlider; // -1 = none, else slider index in panel
	bool captureMouse;
	float grabDist; // distance at which grabbed board floats
	int grabbedIdx; // -1 = none
} App;

static void billboard_bake_svg(Billboard *bb, const CrayonState *cr)
{
	const int TEX_SIZE = 512;

	if (!bb->textureReady)
	{
		bb->rt = LoadRenderTexture(TEX_SIZE, TEX_SIZE);
		bb->tex = bb->rt.texture;
		bb->textureReady = true;
	}

	BeginTextureMode(bb->rt);

	ClearBackground(BLANK);

	draw_svg(
		&bb->doc,
		TEX_SIZE * 0.5f,
		TEX_SIZE * 0.5f,
		TEX_SIZE,
		TEX_SIZE,
		cr,
		1.0f);

	EndTextureMode();
}

static void DrawTexturedQuad(Texture2D tex, Vector3 center, float width,
	float height,Color tint)
{
	float hw = width * 0.5f;
	float hh = height * 0.5f;

	rlSetTexture(tex.id);

	rlBegin(RL_QUADS);

	rlColor4ub(tint.r, tint.g, tint.b, tint.a);

	// top left
	rlTexCoord2f(0.0f, 1.0f);
	rlVertex3f(center.x - hw, center.y + hh, center.z);

	// bottom right
	rlTexCoord2f(0.0f, 0.0f);
	rlVertex3f(center.x - hw, center.y - hh, center.z);

	// bottom right
	rlTexCoord2f(1.0f, 0.0f);
	rlVertex3f(center.x + hw, center.y - hh, center.z);

	// top right
	rlTexCoord2f(1.0f, 1.0f);
	rlVertex3f(center.x + hw, center.y + hh, center.z);

	rlEnd();

	rlSetTexture(0);
}


// zenity file picker (not including one native because too lazy)
static bool pick_file(char *outPath, int maxLen)
{
	FILE *fp = popen("zenity --file-selection --title='Import SVG' "
	"--file-filter='SVG files (*.svg)|*.svg' 2>/dev/null","r");

	if(!fp) return false;

	bool got = (fgets(outPath, maxLen, fp) != NULL);
	pclose(fp);
	if(got)
	{
		char *nl=strchr(outPath,'\n');
		if(nl)
		{
			*nl=0;
		}
	}
	return got && outPath[0]!=0;
}

// GUI HELPERS //

static float draw_slider(float x, float y, float w, const char *label,
float val, float mn, float mx, bool *hovered)
{
	// label
	char buf[64];
	snprintf(buf,sizeof(buf),"%.2f",val);
	DrawText(label,(int)x,(int)y,14,DARKGRAY);
	DrawText(buf,(int)(x+w-60),(int)y,14,DARKGRAY);

	float ty = y+18;
	DrawRectangleRec((Rectangle){x,ty,w,6},(Color){200,200,200,255});
	float t=(val-mn)/(mx-mn);
	DrawRectangleRec((Rectangle){x,ty,(int)(w*t),6},(Color){255,89,100,255});

	// handle
	float hx=x+w*t, hy=ty+3;
	Vector2 mouse=GetMousePosition();
	bool over=(fabsf(mouse.x-hx)<10&&fabsf(mouse.y-hy)<10);
	if(hovered)*hovered=over;
	DrawCircle((int)hx,(int)hy,over?8:6,over?(Color){255,89,100,255}:WHITE);
	DrawCircleLines((int)hx,(int)hy,over?8:6,(Color){180,60,70,255});

	// drag
	if(over && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
	{
		float newt=((mouse.x-x)/w);
		if(newt<0)newt=0;
		if(newt>1)newt=1;
		val=mn+(mx-mn)*newt;
	}
	return val;
}

static void draw_int_slider(float x, float y, float w, const char *label,
int *val, int mn, int mx)
{
	bool hov=false;
	float fv=draw_slider(x,y,w,label,(float)*val,(float)mn,(float)mx,&hov);
	*val=(int)roundf(fv);
}


int main(void)
{
	const int W=1280, H=720;
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
	InitWindow(W,H,"Crayon World");
	SetTargetFPS(60);

	App app = {0};
	// canmera
	app.cam.position = (Vector3){0,1.7f,0};
	app.cam.target = (Vector3){0,1.7f,-5};
	app.cam.up = (Vector3){0,1,0};
	app.cam.fovy = 120;
	app.cam.projection = CAMERA_PERSPECTIVE;
	// crayon defaults
	app.crayon.jitter = 6.0f;
	app.crayon.roughness = 0.04f;
	app.crayon.fps = 8;
	app.crayon.strokeMult = 1.0f;
	app.crayon.colorHue = 0.0f;
	app.crayon.alpha = 1.0f;
	app.grabDist = 4.0f;
	app.grabbedIdx = -1;
	app.captureMouse = true;
	app.activeSlider = -1;

	DisableCursor();

	float crayonTimer = 0;
	float crayonSeed = 0;

	float yaw=-120*DEG2RAD, pitch=0;

	while(!WindowShouldClose())
	{
	float dt=GetFrameTime();

	// crayon seed
	bool seedChanged = false;
	crayonTimer += dt;
	if(crayonTimer >= 1.0f/(float)app.crayon.fps)
	{
		crayonTimer=0;
		crayonSeed = (float)(GetRandomValue(0,99999));
		seedChanged = true;
	}
	app.crayon.seed = crayonSeed;

	if(seedChanged)
	{
		for(int i = 0; i < app.nBoards; i++)
		{
			billboard_bake_svg(
				&app.boards[i],
				&app.crayon
			);
		}
	}

	// mouse look
	if(app.captureMouse && !app.panelOpen)
	{
		Vector2 md=GetMouseDelta();
		yaw += md.x * 0.002f;
		pitch += md.y * 0.002f;
		if(pitch>89*DEG2RAD)
		{
			pitch=89*DEG2RAD;
		}

		if(pitch<-89*DEG2RAD)
		{
			pitch=-89*DEG2RAD;
		}

		Vector3 dir =
		{
			cosf(pitch)*cosf(yaw),
			sinf(-pitch),
			cosf(pitch)*sinf(yaw)
		};
		app.cam.target=Vector3Add(app.cam.position,dir);
	}

	// movement
	if(!app.panelOpen)
	{
		Vector3 fwd=Vector3Normalize(Vector3Subtract(app.cam.target,app.cam.position));
		fwd.y=0; fwd=Vector3Normalize(fwd);
		Vector3 right=Vector3CrossProduct(fwd,(Vector3){0,1,0});
		float spd=5*dt;
		if(IsKeyDown(KEY_W))
			app.cam.position=Vector3Add(app.cam.position,Vector3Scale(fwd, spd));
		if(IsKeyDown(KEY_S))
			app.cam.position=Vector3Add(app.cam.position,Vector3Scale(fwd,-spd));
		if(IsKeyDown(KEY_A))
			app.cam.position=Vector3Add(app.cam.position,Vector3Scale(right,-spd));
		if(IsKeyDown(KEY_D))
			app.cam.position=Vector3Add(app.cam.position,Vector3Scale(right, spd));
		app.cam.target=Vector3Add(app.cam.position,
		(Vector3){cosf(pitch)*cosf(yaw),sinf(-pitch),cosf(pitch)*sinf(yaw)});
	}

	// I-import SVG
	if(IsKeyPressed(KEY_I))
	{
		// fix zenity
		EnableCursor();
		char path[512]={0};
		if(pick_file(path,sizeof(path)) && app.nBoards<MAX_BILLBOARDS)
		{
			Billboard *bb=&app.boards[app.nBoards];
			memset(bb,0,sizeof(*bb));
			if(svg_load(path,&bb->doc))
			{
				billboard_bake_svg(bb, &app.crayon);
				Vector3 fwd=Vector3Normalize(Vector3Subtract(app.cam.target,app.cam.position));
				bb->pos=Vector3Add(app.cam.position,Vector3Scale(fwd,4.0f));
				bb->scale=2.0f;
				app.nBoards++;
			}
		}
		DisableCursor();
		app.captureMouse=true;
	}

	// G-rab nearest board
	if(IsKeyPressed(KEY_G))
	{
		if(app.grabbedIdx>=0)
		{
			app.grabbedIdx=-1;
		}
		else
		{
			// find nearest
			float best=1e9f; int bi=-1;
			for(int i=0;i<app.nBoards;i++)
			{
				float d=Vector3Distance(app.cam.position,app.boards[i].pos);
				if(d<best)
				{
					best=d;bi=i;
				}
			}
			app.grabbedIdx=bi;
			if(bi>=0) app.grabDist=Vector3Distance(app.cam.position,app.boards[bi].pos);
		}
	}

	if(app.grabbedIdx>=0)
	{
		Vector3 fwd=Vector3Normalize(Vector3Subtract(app.cam.target,app.cam.position));
		app.boards[app.grabbedIdx].pos=Vector3Add(app.cam.position,Vector3Scale(fwd,app.grabDist));
		float wheel=GetMouseWheelMove();
		app.grabDist+=wheel*0.3f;
		if(app.grabDist<1.0f)
		{
			app.grabDist=1.0f;
		}
	}

	// Toggle panel
	if(IsKeyPressed(KEY_TAB))
	{
		app.panelOpen=!app.panelOpen;
		if(app.panelOpen)
		{
			EnableCursor(); app.captureMouse=false;
		}
		else
		{
			DisableCursor(); app.captureMouse=true;

		}
	}

	// DRAW
	BeginDrawing();
	ClearBackground((Color){66,66,66,255}); // 3D bg

	BeginMode3D(app.cam);

	// floor grid
	DrawGrid(30,1.0f);

	// billboard quads
	for(int i=0;i<app.nBoards;i++)
	{
		Billboard *bb=&app.boards[i];
		float ratio=(bb->doc.vh>0)?(bb->doc.vw/bb->doc.vh):1;
		float hw=bb->scale*ratio*0.5f;
		float hh=bb->scale*0.5f;
		Color edgeCol = (i==app.grabbedIdx)?(Color){255,200,50,200}:(Color){80,180,255,120};
		DrawLine3D((Vector3){bb->pos.x-hw,bb->pos.y-hh,bb->pos.z},
		(Vector3){bb->pos.x+hw,bb->pos.y-hh,bb->pos.z},edgeCol);
		DrawLine3D((Vector3){bb->pos.x+hw,bb->pos.y-hh,bb->pos.z},
		(Vector3){bb->pos.x+hw,bb->pos.y+hh,bb->pos.z},edgeCol);
		DrawLine3D((Vector3){bb->pos.x+hw,bb->pos.y+hh,bb->pos.z},
		(Vector3){bb->pos.x-hw,bb->pos.y+hh,bb->pos.z},edgeCol);
		DrawLine3D((Vector3){bb->pos.x-hw,bb->pos.y+hh,bb->pos.z},
		(Vector3){bb->pos.x-hw,bb->pos.y-hh,bb->pos.z},edgeCol);
	}

	for(int i = 0; i < app.nBoards; i++)
	{
		Billboard *bb = &app.boards[i];

		if(!bb->textureReady)
		{
			continue;
		}

		float ratio = (bb->doc.vh > 0) ? bb->doc.vw / bb->doc.vh : 1.0f;

		float width = bb->scale * ratio;
		float height = bb->scale;

		DrawTexturedQuad
		(
			bb->tex,
			bb->pos,
			width,
			height,
			WHITE
		);
	}

	EndMode3D();




	// crosshair
	int cx=W/2, cy=H/2;
	DrawLine(cx-12,cy,cx-4,cy,(Color){255,255,255,200});
	DrawLine(cx+4, cy,cx+12,cy,(Color){255,255,255,200});
	DrawLine(cx,cy-12,cx,cy-4,(Color){255,255,255,200});
	DrawLine(cx,cy+4, cx,cy+12,(Color){255,255,255,200});
	DrawCircleLines(cx,cy,3,(Color){255,255,255,140});

	// settings panel
	if(app.panelOpen)
	{
		int px=W-PANEL_WIDTH, py=0;
		DrawRectangle(px,py,PANEL_WIDTH,H,(Color){20,20,25,230});
		DrawRectangleLines(px,py,PANEL_WIDTH,H,(Color){80,80,100,200});

		int tx=px+PANEL_PADDING, ty=PANEL_PADDING;
		DrawText("CRAYON SETTINGS",tx,ty,18,(Color){255,89,100,255}); ty+=32;
		DrawLine(tx,ty,tx+PANEL_WIDTH-PANEL_PADDING*2,ty,(Color){60,60,80,255}); ty+=16;

		float sw=PANEL_WIDTH-PANEL_PADDING*2;
		bool hov=false;

		app.crayon.jitter=draw_slider(tx,ty,sw,"Jitter",app.crayon.jitter,0,25,&hov); ty+=46;
		app.crayon.roughness=draw_slider(tx,ty,sw,"Roughness",app.crayon.roughness,0.005f,0.2f,&hov); ty+=46;
		app.crayon.strokeMult=draw_slider(tx,ty,sw,"Stroke Mult",app.crayon.strokeMult,0.1f,5.0f,&hov); ty+=46;
		draw_int_slider(tx,ty,sw,"Anim FPS",&app.crayon.fps,1,CRAYON_FPS_MAX); ty+=46;

		DrawLine(tx,ty,tx+(int)sw,ty,(Color){60,60,80,255}); ty+=16;
		DrawText("G — grab/release nearest SVG",tx,ty,13,(Color){150,150,170,255}); ty+=20;
		DrawText("I — import SVG",tx,ty,13,(Color){150,150,170,255}); ty+=20;
		DrawText("WASD / mouse — move",tx,ty,13,(Color){150,150,170,255}); ty+=20;
		DrawText("TAB — close panel",tx,ty,13,(Color){150,150,170,255}); ty+=20;
		DrawText("Scroll — push/pull grabbed SVG",tx,ty,13,(Color){150,150,170,255});
	}

	// HUD hints
	if(!app.panelOpen)
	{
		DrawText("[TAB] Settings [I] Import SVG [G] Grab", 10, H-24, 13, (Color){120,120,140,200});
	}
	if(app.grabbedIdx>=0)
	{
		DrawText("SVG GRABBED — [G] to release, scroll to push/pull",
		W/2-200, 20, 14, (Color){255,200,50,220});
	}

		DrawFPS(10,10);
		EndDrawing();
	}

	// cleanup

	for(int i=0;i<app.nBoards;i++)
	{
		svg_free(&app.boards[i].doc);

		if(app.boards[i].textureReady)
		{
			UnloadRenderTexture(app.boards[i].rt);
		}
	}

	CloseWindow();
	return 0;
}
