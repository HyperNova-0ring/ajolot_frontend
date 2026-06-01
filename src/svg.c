#include "ajolot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// COLOR HELPERS //
static Color parse_hex_color(const char *s, bool *ok)
{
	while (*s == ' ')
	{
		s++;
	}
	if (*s == '#')
	{
		s++;
	}
	if (ok)
	{
		*ok = true;
	}
	unsigned int r = 0, g = 0, b = 0, a = 255;
	if (strlen(s) >= 6)
	{
		sscanf(s, "%2x%2x%2x", &r, &g, &b);
	}
	else if (strlen(s) == 3)
	{
		char ex[7] = {s[0], s[0], s[1], s[1], s[2], s[2], 0};
		sscanf(ex, "%2x%2x%2x", &r, &g, &b);
	}
	else
	{
		if (ok)
		{
			*ok = false;
		}
		return BLACK;
	}
	return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
}

static Color named_color(const char *name, bool *ok)
{
	if (ok)
	{
		*ok = true;
	}
	if (!strcmp(name, "black"))
	{
		return BLACK;
	}
	if (!strcmp(name, "white"))
	{
		return WHITE;
	}
	if (!strcmp(name, "red"))
	{
		return RED;
	}
	if (!strcmp(name, "green"))
	{
		return GREEN;
	}
	if (!strcmp(name, "blue"))
	{
		return BLUE;
	}
	if (!strcmp(name, "yellow"))
	{
		return YELLOW;
	}
	if (!strcmp(name, "orange"))
	{
		return ORANGE;
	}
	if (!strcmp(name, "gray") || !strcmp(name, "grey"))
	{
		return GRAY;
	}
	if (!strcmp(name, "none"))
	{
		if (ok)
		{
			*ok = false;
		}
		return BLANK;
	}
	if (*name == '#')
	{
		return parse_hex_color(name, ok);
	}
	if (ok)
	{
		*ok = false;
	}
	return BLACK;
}

static Color parse_color(const char *s, bool *ok)
{
	while (*s == ' ')
	{
		s++;
	}
	if (!*s || !strcmp(s, "none"))
	{
		if (ok)
		{
			*ok = false;
		}
		return BLANK;
	}
	if (*s == '#')
	{
		return parse_hex_color(s, ok);
	}
	return named_color(s, ok);
}

// CRAYON DISPLACEMENT
// noise hash
static float hash2(float x, float y, float seed)
{
	// integer-grid noise, bilinear
	int ix = (int)floorf(x);
	int iy = (int)floorf(y);
	float fx = x - ix;
	float fy = y - iy;
	fx = fx * fx * (3 - 2 * fx);
	fy = fy * fy * (3 - 2 * fy);
	float s = seed * 127.1f;
	float a = sinf((ix + iy * 57.0f + s) * 127.1f) * 43758.5453f;
	float b = sinf((ix + 1 + iy * 57.0f + s) * 127.1f) * 43758.5453f;
	float c = sinf((ix + (iy + 1) * 57.0f + s) * 127.1f) * 43758.5453f;
	float d = sinf((ix + 1 + (iy + 1) * 57.0f + s) * 127.1f) * 43758.5453f;
	a -= floorf(a);
	b -= floorf(b);
	c -= floorf(c);
	d -= floorf(d);
	return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

static Vector2 crayon_displace(Vector2 p, const CrayonState *cr)
{
	float freq = cr->roughness * 4.0f;
	float nx = hash2(p.x * freq, p.y * freq, cr->seed) * 2.0f - 1.0f;
	float ny = hash2(p.x * freq, p.y * freq, cr->seed + 17.3f) * 2.0f - 1.0f;

	nx += 0.5f * (hash2(p.x * freq * 2, p.y * freq * 2, cr->seed + 3.1f) * 2 - 1);
	ny += 0.5f * (hash2(p.x * freq * 2, p.y * freq * 2, cr->seed + 9.7f) * 2 - 1);
	return (Vector2){p.x + nx * cr->jitter, p.y + ny * cr->jitter};
}

static void emit_point(float x, float y, Vector2 *out, int maxPts, int *n, float *cx, float *cy)
{
	if (*n < maxPts)
	{
		out[*n].x = x;
		out[*n].y = y;
		(*n)++;
	}
	*cx = x;
	*cy = y;
}

// PATH PARSER
static int path_to_polyline(const char *d, Vector2 *out, int maxPts)
{
	int n = 0;
	float cx = 0, cy = 0, sx = 0, sy = 0;
	float lastcx = 0, lastcy = 0;
	char cmd = 0;
	const char *p = d;
	bool prevWasBezier = false;

	while (*p)
	{
		while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t')
		{
			p++;
		}
		if (!*p)
		{
			break;
		}

		// command letter?
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
		{
			cmd = *p++;
			if (cmd == 'Z' || cmd == 'z')
			{
				emit_point(sx, sy, out, maxPts, &n, &cx, &cy);
				prevWasBezier = false;
				continue;
			}
			continue;
		}

		// read numbers based on cmd
		float args[7];
		int nArgs = 0;
		int need = 2;
		switch (cmd)
		{
			case 'H':
			case 'h':
			case 'V':
			case 'v':
				need = 1;
				break;
			case 'C':
			case 'c':
				need = 6;
				break;
			case 'S':
			case 's':
			case 'Q':
			case 'q':
				need = 4;
				break;
			case 'T':
			case 't':
				need = 2;
				break;
			case 'A':
			case 'a':
				need = 7;
				break;
			default:
				need = 2;
		}
		// need floats
		for (int i = 0; i < need; i++)
		{
			while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t')
			{
				p++;
			}
			if (!*p || ((*p < '0' || *p > '9') && *p != '-' && *p != '+'))
			{
				break;
			}
			args[i] = strtof(p, (char **)&p);
			nArgs++;
		}
		if (nArgs == 0)
		{
			p++;
			continue;
		}

		if (!prevWasBezier)
		{
			lastcx = cx;
			lastcy = cy;
		}
		prevWasBezier = false;

		switch (cmd)
		{
			case 'M':
				emit_point(args[0], args[1], out, maxPts, &n, &cx, &cy);
				sx = cx;
				sy = cy;
				cmd = 'L';
				break;
			case 'm':
				emit_point(cx + args[0], cy + args[1], out, maxPts, &n, &cx, &cy);
				sx = cx;
				sy = cy;
				cmd = 'l';
				break;
			case 'L':
				emit_point(args[0], args[1], out, maxPts, &n, &cx, &cy);
				break;
			case 'l':
				emit_point(cx + args[0], cy + args[1], out, maxPts, &n, &cx, &cy);
				break;
			case 'H':
				emit_point(args[0], cy, out, maxPts, &n, &cx, &cy);
				break;
			case 'h':
				emit_point(cx + args[0], cy, out, maxPts, &n, &cx, &cy);
				break;
			case 'V':
				emit_point(cx, args[0], out, maxPts, &n, &cx, &cy);
				break;
			case 'v':
				emit_point(cx, cy + args[0], out, maxPts, &n, &cx, &cy);
				break;
			case 'C':
			case 'c':
			{
				// cubic bezier: tesselate with ~8 steps
				float x1 = (cmd == 'C') ? args[0] : cx + args[0];
				float y1 = (cmd == 'C') ? args[1] : cy + args[1];
				float x2 = (cmd == 'C') ? args[2] : cx + args[2];
				float y2 = (cmd == 'C') ? args[3] : cy + args[3];
				float ex = (cmd == 'C') ? args[4] : cx + args[4];
				float ey = (cmd == 'C') ? args[5] : cy + args[5];
				lastcx = x2;
				lastcy = y2;
				prevWasBezier = true;
				int steps = 10;
				for (int s = 1; s <= steps; s++)
				{
					float t = (float)s / steps;
					float u = 1 - t;
					float bx = u * u * u * cx + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * ex;
					float by = u * u * u * cy + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * ey;
					emit_point(bx, by, out, maxPts, &n, &cx, &cy);
				}
				break;
			}
			case 'S':
			case 's':
			{
				float x1 = 2 * cx - lastcx, y1 = 2 * cy - lastcy;
				float x2 = (cmd == 'S') ? args[0] : cx + args[0];
				float y2 = (cmd == 'S') ? args[1] : cy + args[1];
				float ex = (cmd == 'S') ? args[2] : cx + args[2];
				float ey = (cmd == 'S') ? args[3] : cy + args[3];
				lastcx = x2;
				lastcy = y2;
				prevWasBezier = true;
				int steps = 10;
				for (int s = 1; s <= steps; s++)
				{
					float t = (float)s / steps, u = 1 - t;
					float bx = u * u * u * cx + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * ex;
					float by = u * u * u * cy + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * ey;
					emit_point(bx, by, out, maxPts, &n, &cx, &cy);
				}
				break;
			}
			case 'Q':
			case 'q':
			{
				float x1 = (cmd == 'Q') ? args[0] : cx + args[0];
				float y1 = (cmd == 'Q') ? args[1] : cy + args[1];
				float ex = (cmd == 'Q') ? args[2] : cx + args[2];
				float ey = (cmd == 'Q') ? args[3] : cy + args[3];
				lastcx = x1;
				lastcy = y1;
				prevWasBezier = true;
				int steps = 8;
				for (int s = 1; s <= steps; s++)
				{
					float t = (float)s / steps, u = 1 - t;
					float bx = u * u * cx + 2 * u * t * x1 + t * t * ex;
					float by = u * u * cy + 2 * u * t * y1 + t * t * ey;
					emit_point(bx, by, out, maxPts, &n, &cx, &cy);
				}
				break;
			}
			case 'A':
			case 'a':
			{
				float rx = fabsf(args[0]);
				float ry = fabsf(args[1]);
				float phi = args[2] * PI / 180.0f;
				int large_arc = (int)(args[3] + 0.5f);
				int sweep_flag = (int)(args[4] + 0.5f);
				float x2 = (cmd == 'A') ? args[5] : cx + args[5];
				float y2 = (cmd == 'A') ? args[6] : cy + args[6];
				if (rx < 1e-6f || ry < 1e-6f) {
					emit_point(x2, y2, out, maxPts, &n, &cx, &cy);
					break;
				}
				float cos_phi = cosf(phi), sin_phi = sinf(phi);
				float dx2 = (cx - x2) * 0.5f, dy2 = (cy - y2) * 0.5f;
				float x1p =  cos_phi * dx2 + sin_phi * dy2;
				float y1p = -sin_phi * dx2 + cos_phi * dy2;
				float x1p2 = x1p * x1p, y1p2 = y1p * y1p;
				float rx2 = rx * rx, ry2 = ry * ry;
				float lam = x1p2 / rx2 + y1p2 / ry2;
				if (lam > 1.0f) {
					float sq = sqrtf(lam);
					rx *= sq; ry *= sq;
					rx2 = rx * rx; ry2 = ry * ry;
				}
				float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
				float den = rx2 * y1p2 + ry2 * x1p2;
				float coeff = (den < 1e-10f) ? 0.0f : sqrtf(fabsf(num / den));
				if (large_arc == sweep_flag) coeff = -coeff;
				float cxp =  coeff * rx * y1p / ry;
				float cyp = -coeff * ry * x1p / rx;
				float arc_cx = cos_phi * cxp - sin_phi * cyp + (cx + x2) * 0.5f;
				float arc_cy = sin_phi * cxp + cos_phi * cyp + (cy + y2) * 0.5f;
				float ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry;
				float vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
				float lu = sqrtf(ux*ux + uy*uy);
				float theta1 = (lu > 0) ? (uy >= 0 ? acosf(fmaxf(-1.f, fminf(1.f, ux / lu)))
				                                     : -acosf(fmaxf(-1.f, fminf(1.f, ux / lu)))) : 0.f;
				float luv = sqrtf((ux*ux + uy*uy) * (vx*vx + vy*vy));
				float dtheta = (luv > 0) ? acosf(fmaxf(-1.f, fminf(1.f, (ux*vx + uy*vy) / luv))) : 0.f;
				if (ux * vy - uy * vx < 0) dtheta = -dtheta;
				if (sweep_flag == 0 && dtheta > 0) dtheta -= 2.f * PI;
				if (sweep_flag == 1 && dtheta < 0) dtheta += 2.f * PI;
				int steps = (int)(fabsf(dtheta) * 8.0f / PI) + 2;
				if (steps > 64) steps = 64;
				for (int s = 1; s <= steps; s++) {
					float ang = theta1 + dtheta * (float)s / steps;
					float px = cos_phi * cosf(ang) * rx - sin_phi * sinf(ang) * ry + arc_cx;
					float py = sin_phi * cosf(ang) * rx + cos_phi * sinf(ang) * ry + arc_cy;
					emit_point(px, py, out, maxPts, &n, &cx, &cy);
				}
				break;
			}
		}
	}
	return n;
}

// attr parser
static bool attr_get(const char *tag_text, const char *name, char *out, int maxLen)
{
	const char *p = strstr(tag_text, name);
	while (p)
	{
		// make sure it's an actual attr= boundary
		char before = (p == tag_text) ? ' ' : *(p - 1);
		if (before == ' ' || before == '\n' || before == '\t' || before == '\r')
		{
			p += strlen(name);
			while (*p == ' ')
			{
				p++;
			}
			if (*p == '=')
			{
				p++;
				while (*p == ' ')
				{
					p++;
				}
				char delim = (*p == '"' || *p == '\'') ? *p++ : ' ';
				int i = 0;
				while (*p && *p != delim && i < maxLen - 1)
				{
					out[i++] = *p++;
				}
				out[i] = 0;
				return true;
			}
		}
		p = strstr(p + 1, name);
	}
	return false;
}

static int parse_points(const char *pts, Vector2 *out, int max)
{
	int n = 0;
	const char *p = pts;
	while (*p && n < max)
	{
		while (*p == ' ' || *p == '\n' || *p == '\t')
		{
			p++;
		}
		if (!*p)
		{
			break;
		}
		float x = strtof(p, (char **)&p);
		while (*p == ',' || *p == ' ')
		{
			p++;
		}
		float y = strtof(p, (char **)&p);
		out[n++] = (Vector2){x, y};
	}
	return n;
}

static void fill_polygon(Vector2 *pts, int n, Color color)
{
	if (n < 3) return;

	// Compute signed area to determine winding in screen space (y-down).
	// Positive area = CW in screen; after raylib's ortho y-flip, screen-CW
	// becomes NDC-CW = back face = culled. Flip those to CCW before drawing.
	float area = 0;
	for (int i = 0; i < n; i++)
	{
		int j = (i + 1) % n;
		area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
	}
	bool cw = (area >= 0); // true = polygon is CW in screen space

	if (n == 3)
	{
		if (cw) DrawTriangle(pts[2], pts[1], pts[0], color);
		else    DrawTriangle(pts[0], pts[1], pts[2], color);
		return;
	}

	int idx[2048];
	int rem = n < 2048 ? n : 2047;
	for (int i = 0; i < rem; i++) idx[i] = i;

	int cur = 0;
	int max_iter = rem * rem;

	for (int iter = 0; rem > 3 && iter < max_iter; iter++)
	{
		int pi = (cur + rem - 1) % rem;
		int ni = (cur + 1) % rem;
		Vector2 a = pts[idx[pi]];
		Vector2 b = pts[idx[cur]];
		Vector2 c = pts[idx[ni]];

		float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
		bool ear = cw ? (cross >= 0) : (cross <= 0);

		for (int k = 0; k < rem && ear; k++)
		{
			if (k == pi || k == cur || k == ni) continue;
			Vector2 p = pts[idx[k]];
			float d1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
			float d2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
			float d3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
			bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
			bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
			if (!(neg && pos)) ear = false;
		}

		if (ear)
		{
			// Emit screen-CCW so raylib's ortho y-flip yields NDC-CCW = front face.
			if (cw) DrawTriangle(c, b, a, color);
			else    DrawTriangle(a, b, c, color);
			for (int k = cur; k < rem - 1; k++) idx[k] = idx[k + 1];
			rem--;
			if (cur >= rem) cur = 0;
		}
		else
		{
			cur = (cur + 1) % rem;
		}
	}

	if (rem == 3)
	{
		if (cw) DrawTriangle(pts[idx[2]], pts[idx[1]], pts[idx[0]], color);
		else    DrawTriangle(pts[idx[0]], pts[idx[1]], pts[idx[2]], color);
	}
}

// SVG LOADER //
void draw_svg(const SvgDoc *doc, float screenX, float screenY,
	float drawW, float drawH, const CrayonState *cr, float strokeMult)
{
	if (doc->nShapes == 0)
	{
		return;
	}

	// real svg bounds
	float minX = 1e30f;
	float minY = 1e30f;
	float maxX = -1e30f;
	float maxY = -1e30f;

	for (int s = 0; s < doc->nShapes; s++)
	{
		const SvgShape *sh = &doc->shapes[s];

		for (int i = 0; i < sh->nPts; i++)
		{
			Vector2 p = sh->pts[i];

			if (p.x < minX)
			{
				minX = p.x;
			}
			if (p.y < minY)
			{
				minY = p.y;
			}
			if (p.x > maxX)
			{
				maxX = p.x;
			}
			if (p.y > maxY)
			{
				maxY = p.y;
			}
		}
	}

	float svgW = maxX - minX;
	float svgH = maxY - minY;

	if (svgW <= 0)
	{
		svgW = 1;
	}
	if (svgH <= 0)
	{
		svgH = 1;
	}

	const float padding = 0.92f;

	float scale = fminf(drawW / svgW, drawH / svgH) * padding;

	float fittedW = svgW * scale;
	float fittedH = svgH * scale;

	float offX = screenX - fittedW * 0.5f;
	float offY = screenY - fittedH * 0.5f;

	static Vector2 base[2048];
	static Vector2 disp[2048];

	for (int s = 0; s < doc->nShapes; s++)
	{
		const SvgShape *sh = &doc->shapes[s];

		if (sh->nPts < 2)
		{
			continue;
		}

		int n = sh->nPts < 2048 ? sh->nPts : 2047;

		// displacement
		for (int i = 0; i < n; i++)
		{
			Vector2 p = sh->pts[i];

			// normalize to bounds
			p.x -= minX;
			p.y -= minY;

			// scale to square
			p.x *= scale;
			p.y *= scale;

			// center
			p.x += offX;
			p.y += offY;

			base[i] = p;
			disp[i] = crayon_displace(p, cr);
		}

		float sw = sh->strokeWidth * scale * strokeMult;

		if (sw < 1.0f)
		{
			sw = 1.0f;
		}

		float a = (cr->alpha <= 0.0f) ? 0.0f : (cr->alpha >= 1.0f) ? 1.0f : cr->alpha;

		// fill — SVG fills open paths too (implicit closepath for fill purposes)
		if (sh->hasFill && n >= 3)
		{
			Color fc = sh->fill;
			fc.a = (unsigned char)(fc.a * a);
			fill_polygon(base, n, fc);
		}

		// stroke
		if (sh->hasStroke || (!sh->hasFill))
		{
			Color sc = sh->hasStroke ? sh->stroke : DARKGRAY;
			sc.a = (unsigned char)(sc.a * a);

			int lim = sh->closed ? n : n - 1;

			for (int i = 0; i < lim; i++)
			{
				Vector2 a = disp[i];
				Vector2 b = disp[(i + 1) % n];

				DrawLineEx(a, b, sw, sc);
			}
		}
	}
}

void svg_free(SvgDoc *doc)
{
	for (int i = 0; i < doc->nShapes; i++)
	{
		if (doc->shapes[i].pts)
		{
			free(doc->shapes[i].pts);
		}
	}
	doc->nShapes = 0;
}

static void apply_style(SvgShape *sh, const char *style)
{
	char buf[512];
	strncpy(buf, style, 511);
	buf[511] = 0;
	char *tok = strtok(buf, ";");
	while (tok)
	{
		char *colon = strchr(tok, ':');
		if (colon)
		{
			*colon = 0;
			char *key = tok;
			while (*key == ' ')
			{
				key++;
			}
			char *val = colon + 1;
			while (*val == ' ')
			{
				val++;
			}
			bool ok;
			if (!strcmp(key, "fill"))
			{
				sh->fill = parse_color(val, &ok);
				sh->hasFill = ok;
			}
			if (!strcmp(key, "stroke"))
			{
				sh->stroke = parse_color(val, &ok);
				sh->hasStroke = ok;
			}
			if (!strcmp(key, "stroke-width"))
			{
				sh->strokeWidth = atof(val);
			}
			// TODO: OPTIONAL: implement rounded lines, should use draw_stroked_polyline?
			if (!strcmp(key, "stroke-linecap"))
			{
				if (!strcmp(val, "round"))
					sh->lineCap = CAP_ROUND;
				else if (!strcmp(val, "square"))
					sh->lineCap = CAP_SQUARE;
				else
					sh->lineCap = CAP_BUTT;
			}
			if (!strcmp(key, "stroke-linejoin"))
			{
				if (!strcmp(val, "round"))
					sh->lineJoin = JOIN_ROUND;
				else if (!strcmp(val, "bevel"))
					sh->lineJoin = JOIN_BEVEL;
				else
					sh->lineJoin = JOIN_MITER;
			}
		}
		tok = strtok(NULL, ";");
	}
}

static void apply_transform(Vector2 *pts, int n, const char *tf)
{
	while (*tf == ' ') tf++;
	if (strncmp(tf, "matrix(", 7) == 0) {
		float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
		sscanf(tf + 7, "%f,%f,%f,%f,%f,%f", &a, &b, &c, &d, &e, &f);
		for (int i = 0; i < n; i++) {
			float x = pts[i].x, y = pts[i].y;
			pts[i].x = a * x + c * y + e;
			pts[i].y = b * x + d * y + f;
		}
	} else if (strncmp(tf, "translate(", 10) == 0) {
		float tx = 0, ty = 0;
		sscanf(tf + 10, "%f,%f", &tx, &ty);
		for (int i = 0; i < n; i++) { pts[i].x += tx; pts[i].y += ty; }
	} else if (strncmp(tf, "scale(", 6) == 0) {
		float sx = 1, sy = 1;
		int got = sscanf(tf + 6, "%f,%f", &sx, &sy);
		if (got < 2) sy = sx;
		for (int i = 0; i < n; i++) { pts[i].x *= sx; pts[i].y *= sy; }
	} else if (strncmp(tf, "rotate(", 7) == 0) {
		float angle = 0, rcx = 0, rcy = 0;
		sscanf(tf + 7, "%f,%f,%f", &angle, &rcx, &rcy);
		angle *= PI / 180.0f;
		float ca = cosf(angle), sa = sinf(angle);
		for (int i = 0; i < n; i++) {
			float x = pts[i].x - rcx, y = pts[i].y - rcy;
			pts[i].x = ca * x - sa * y + rcx;
			pts[i].y = sa * x + ca * y + rcy;
		}
	}
}

static bool svg_parse(char *raw, SvgDoc *doc)
{
	// get viewbox
	{
		char vb[64] = {0};
		if (attr_get(raw, "viewBox", vb, sizeof(vb)))
		{
			float x, y, w, h;
			if (sscanf(vb, "%f %f %f %f", &x, &y, &w, &h) == 4)
			{
				doc->vw = w;
				doc->vh = h;
			}
		}
		else
		{
			// test w, h
			char tmp[32] = {0};
			if (attr_get(raw, "width", tmp, sizeof(tmp)))
			{
				doc->vw = atof(tmp);
			}
			if (attr_get(raw, "height", tmp, sizeof(tmp)))
			{
				doc->vh = atof(tmp);
			}
		}
	}

	// tags
	const char *p = raw;
	while ((p = strchr(p, '<')) != NULL)
	{
		p++;
		if (*p == '/')
		{
			p++;
			continue;
		}
		if (*p == '!')
		{
			continue;
		}

		const char *end = strchr(p, '>');
		if (!end)
		{
			break;
		}
		long taglen = end - p;
		if (taglen > 4096)
		{
			p = end;
			continue;
		}
		char tag[4097];
		strncpy(tag, p, taglen);
		tag[taglen] = 0;

		int kind = -1;
		if (strncmp(tag, "path", 4) == 0)
		{
			kind = SHP_PATH;
		}
		else if (strncmp(tag, "circle", 6) == 0)
		{
			kind = SHP_CIRCLE;
		}
		else if (strncmp(tag, "rect", 4) == 0)
		{
			kind = SHP_RECT;
		}
		else if (strncmp(tag, "ellipse", 7) == 0)
		{
			kind = SHP_ELLIPSE;
		}
		else if (strncmp(tag, "line ", 5) == 0)
		{
			kind = SHP_LINE;
		}
		else if (strncmp(tag, "polyline", 8) == 0)
		{
			kind = SHP_POLYLINE;
		}
		else if (strncmp(tag, "polygon", 7) == 0)
		{
			kind = SHP_POLYGON;
		}

		if (kind >= 0 && doc->nShapes < MAX_SVG_SHAPES)
		{
			SvgShape *sh = &doc->shapes[doc->nShapes];
			memset(sh, 0, sizeof(*sh));
			sh->kind = (ShapeKind)kind;
			sh->fill = (Color){0, 0, 0, 255};
			sh->stroke = (Color){0, 0, 0, 255};
			sh->strokeWidth = 1.0f;
			sh->hasFill = true;
			sh->hasStroke = false;
			sh->closed = false;

			// common attr
			char tmp[512] = {0};
			bool ok;
			if (attr_get(tag, "fill", tmp, sizeof(tmp)))
			{
				sh->fill = parse_color(tmp, &ok);
				sh->hasFill = ok;
			}
			if (attr_get(tag, "stroke", tmp, sizeof(tmp)))
			{
				sh->stroke = parse_color(tmp, &ok);
				sh->hasStroke = ok;
			}
			if (attr_get(tag, "stroke-width", tmp, sizeof(tmp)))
			{
				sh->strokeWidth = atof(tmp);
			}
			if (attr_get(tag, "style", tmp, sizeof(tmp)))
			{
				apply_style(sh, tmp);
			}
			if (attr_get(tag, "stroke-linecap", tmp, sizeof(tmp)))
			{
				if (!strcmp(tmp, "round"))
					sh->lineCap = CAP_ROUND;
				else if (!strcmp(tmp, "square"))
					sh->lineCap = CAP_SQUARE;
				else
					sh->lineCap = CAP_BUTT;
			}
			if (attr_get(tag, "stroke-linejoin", tmp, sizeof(tmp)))
			{
				if (!strcmp(tmp, "round"))
					sh->lineJoin = JOIN_ROUND;
				else if (!strcmp(tmp, "bevel"))
					sh->lineJoin = JOIN_BEVEL;
				else
					sh->lineJoin = JOIN_MITER;
			}

			// geometry
			int maxPts = 1024;
			sh->pts = (Vector2 *)malloc(sizeof(Vector2) * maxPts);

			if (kind == SHP_PATH)
			{
				char d[8192] = {0};
				if (attr_get(tag, "d", d, sizeof(d)))
				{
					sh->nPts = path_to_polyline(d, sh->pts, maxPts);
					char *z = strrchr(d, 'Z');
					char *zl = strrchr(d, 'z');
					sh->closed = (z != NULL || zl != NULL);
				}
			}
			else if (kind == SHP_CIRCLE)
			{
				float cx = 0, cy = 0, r = 0;
				if (attr_get(tag, "cx", tmp, sizeof(tmp)))
				{
					cx = atof(tmp);
				}
				if (attr_get(tag, "cy", tmp, sizeof(tmp)))
				{
					cy = atof(tmp);
				}
				if (attr_get(tag, "r", tmp, sizeof(tmp)))
				{
					r = atof(tmp);
				}
				int steps = 36;
				for (int i = 0; i < steps; i++)
				{
					float a = 2 * PI * i / steps;
					sh->pts[sh->nPts++] = (Vector2){cx + cosf(a) * r, cy + sinf(a) * r};
				}
				sh->closed = true;
			}
			else if (kind == SHP_ELLIPSE)
			{
				float cx = 0, cy = 0, rx = 0, ry = 0;
				if (attr_get(tag, "cx", tmp, sizeof(tmp)))
				{
					cx = atof(tmp);
				}
				if (attr_get(tag, "cy", tmp, sizeof(tmp)))
				{
					cy = atof(tmp);
				}
				if (attr_get(tag, "rx", tmp, sizeof(tmp)))
				{
					rx = atof(tmp);
				}
				if (attr_get(tag, "ry", tmp, sizeof(tmp)))
				{
					ry = atof(tmp);
				}
				int steps = 36;
				for (int i = 0; i < steps; i++)
				{
					float a = 2 * PI * i / steps;
					sh->pts[sh->nPts++] = (Vector2){cx + cosf(a) * rx, cy + sinf(a) * ry};
				}
				sh->closed = true;
			}
			else if (kind == SHP_RECT)
			{
				float x = 0, y = 0, w = 0, h = 0;
				if (attr_get(tag, "x", tmp, sizeof(tmp)))
				{
					x = atof(tmp);
				}
				if (attr_get(tag, "y", tmp, sizeof(tmp)))
				{
					y = atof(tmp);
				}
				if (attr_get(tag, "width", tmp, sizeof(tmp)))
				{
					w = atof(tmp);
				}
				if (attr_get(tag, "height", tmp, sizeof(tmp)))
				{
					h = atof(tmp);
				}
				sh->pts[0] = (Vector2){x, y};
				sh->pts[1] = (Vector2){x + w, y};
				sh->pts[2] = (Vector2){x + w, y + h};
				sh->pts[3] = (Vector2){x, y + h};
				sh->nPts = 4;
				sh->closed = true;
			}
			else if (kind == SHP_LINE)
			{
				float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
				if (attr_get(tag, "x1", tmp, sizeof(tmp)))
				{
					x1 = atof(tmp);
				}
				if (attr_get(tag, "y1", tmp, sizeof(tmp)))
				{
					y1 = atof(tmp);
				}
				if (attr_get(tag, "x2", tmp, sizeof(tmp)))
				{
					x2 = atof(tmp);
				}
				if (attr_get(tag, "y2", tmp, sizeof(tmp)))
				{
					y2 = atof(tmp);
				}
				sh->pts[0] = (Vector2){x1, y1};
				sh->pts[1] = (Vector2){x2, y2};
				sh->nPts = 2;
				sh->closed = false;
			}
			else if (kind == SHP_POLYLINE || kind == SHP_POLYGON)
			{
				if (attr_get(tag, "points", tmp, sizeof(tmp)))
				{
					sh->nPts = parse_points(tmp, sh->pts, maxPts);
				}
				sh->closed = (kind == SHP_POLYGON);
			}

			char tf[256] = {0};
			if (attr_get(tag, "transform", tf, sizeof(tf)))
			{
				apply_transform(sh->pts, sh->nPts, tf);
			}

			if (sh->nPts > 0)
			{
				doc->nShapes++;
			}
			else
			{
				free(sh->pts);
				sh->pts = NULL;
			}
		}
		p = end + 1;
	}
	free(raw);
	return doc->nShapes > 0;
}

bool svg_load(const char *path, SvgDoc *doc)
{
	svg_free(doc);
	doc->vw = 100;
	doc->vh = 100;

	FILE *f = fopen(path, "r");
	if (!f)
	{
		return false;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	rewind(f);
	char *raw = (char *)malloc(sz + 1);
	fread(raw, 1, sz, f);
	raw[sz] = 0;
	fclose(f);

	return svg_parse(raw, doc);
}

bool svg_load_from_memory(const unsigned char *data, int size, SvgDoc *doc)
{
	svg_free(doc);
	doc->vw = 100;
	doc->vh = 100;

	char *raw = (char *)malloc(size + 1);
	if (!raw)
	{
		return false;
	}
	memcpy(raw, data, size);
	raw[size] = 0;

	return svg_parse(raw, doc);
}
