// _____________________________________________________________________________
// |                                                                           |
// |  Rotating tetrahedron demo                                                |
// |  =========================                                                |
// |                                                                           |
// | Draws a rotating tetrahedron using the DMC-1 GPU                          |
// |                                                                           |
// | @sylefeb 2022-04-18  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "sine_table.h"
#include "api.c"
#include "raster.c"

// -----------------------------------------------------
// Rotations
// -----------------------------------------------------

static inline void rot_z(int angle, int *x, int *y, int *z)
{
  int sin = sine_table[ angle         & 4095];
  int cos = sine_table[(angle + 1024) & 4095];
  int tx = *x; int ty = *y;
  *x = (cos * tx - sin * ty) >> 12;
  *y = (sin * tx + cos * ty) >> 12;
}

static inline void rot_y(int angle, int *x, int *y, int *z)
{
  int sin = sine_table[ angle         & 4095];
  int cos = sine_table[(angle + 1024) & 4095];
  int tx = *x; int tz = *z;
  *x = (cos * tx - sin * tz) >> 12;
  *z = (sin * tx + cos * tz) >> 12;
}

// -----------------------------------------------------
// Global state
// -----------------------------------------------------

// frame
volatile int frame;

// -----------------------------------------------------

int view_dist = 600;

static inline void project(const p3d* pt, p3d *pr)
{
	int z     = pt->z + view_dist; // view space
	int inv_z = 65536 / z;
	pr->x = ((pt->x * inv_z) >> 8) + SCREEN_WIDTH/2;
	pr->y = ((pt->y * inv_z) >> 8) + SCREEN_HEIGHT/2;
}

// -----------------------------------------------------

const p3d points[4] = {
  { 188, -66,    0},
  { -94, -66,  163},
  { -94, -66, -163},
  {   0, 200,    0},
};

const int indices[12] = {
  0,1,2,
  0,1,3,
  1,2,3,
  2,0,3,
};

p3d     trsf_points[4];
surface srfs[4];

// -----------------------------------------------------

int angle;

static inline void transform(int *x, int *y, int *z)
{
  rot_y(angle   , x,y,z);
  rot_z(angle>>1, x,y,z);
}

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{
  // animation angle
  angle = frame << 6;

	// transform the points
 	for (int i = 0; i < 4; ++i) {
    p3d p = points[i];
    transform(&p.x,&p.y,&p.z);
		project(&p, &trsf_points[i]);
 	}
  trsf_surface tsrfs[4];
	rconvex      rtris[4];
  const int *idx =  indices;
  for (int s = 0 ; s < 4 ; ++ s) {
    // transform the textured surfaces for rendering at this frame
    surface_transform(&srfs[s], &tsrfs[s], view_dist, transform, points);
    // prepare triangle rasterization
	  rconvex_init(&rtris[s], 3,idx, trsf_points);
    idx += 3;
  }

	// before drawing cols wait for previous frame
  wait_all_drawn();

  int bound_s = -1;
  for (int c = 0; c < SCREEN_WIDTH ; ++c) {

    const int *idx =  indices;
    for (int s = 0 ; s < 4 ; ++ s) {

      if (c >= rtris[s].x && c <= rtris[s].last_x && tsrfs[s].ded > 0) {

        rconvex_step(&rtris[s],  3,idx, trsf_points);

        int rz = 256;
        int rx = c           - SCREEN_WIDTH/2;
        int ry = rtris[s].ys - SCREEN_HEIGHT/2;

        if (s != bound_s) {
          surface_bind    (&tsrfs[s]);
          bound_s = s;
        }

        int light = (tsrfs[s].nz>>4);
        if (light > 15) light = 15;

        surface_set_span(&tsrfs[s], rx,ry,rz);
        col_send(
          COLDRAW_PLANE_B(tsrfs[s].ded,tsrfs[s].dr),
          COLDRAW_COL(125/*texture id*/, rtris[s].ys,rtris[s].ye, light) | PLANE
        );

      }

      // next triangle
      idx += 3;

      // process pending column commands
      col_process();
    }

    // background filler
    col_send(
      COLDRAW_WALL(Y_MAX,0,0),
      COLDRAW_COL(0, 0,239, 0) | WALL
    );

    // send end of column
    col_send(0, COLDRAW_EOC);

  }
}

// -----------------------------------------------------
// main
// -----------------------------------------------------

void main_1()
{
  // hangs core 1 (we don't use it yet!)
  while (1) { }
}

void main_0()
{
  *LEDS = 0;

  // --------------------------
  // intialize view and frame
  // --------------------------
  frame    = 0;

  // --------------------------
  // init oled
  // --------------------------
  oled_init();
  oled_fullscreen();

  // --------------------------
  // prepare the texturing surfaces
  // --------------------------
  const int *idx = indices;
  for (int s = 0; s < 4 ; ++s) {
    surface_pre(&srfs[s], *idx,*(idx+1),*(idx+2), points);
    idx += 3;
  }

  // prepare the rasterizer
  raster_pre();

  // --------------------------
  // render loop
  // --------------------------
  while (1) {

    // printf("frame %d\n",frame);

    // animate view distance
    view_dist = 460 + (sine_table[(frame<<5)&4095]>>4);

    // draw screen
    render_frame();

    // next frame
    ++ frame;

  } // while (1)

}

// -----------------------------------------------------

void main()
{
  if (core_id()) {
		main_1();
	} else {
		main_0();
	}
}

// -----------------------------------------------------
