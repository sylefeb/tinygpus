// _____________________________________________________________________________
// |                                                                           |
// |  Interleaved rotating tetrahedrons demo                                   |
// |  ======================================                                   |
// |                                                                           |
// | Draws two rotating tetrahedrons using the DMC-1 GPU                       |
// |                                                                           |
// | @sylefeb 2022-07-26  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "sine_table.h"
#include "api.c"
#include "raster.c"

// -----------------------------------------------------
// Rotations
// -----------------------------------------------------

static inline void rot_z(int angle, short *x, short *y, short *z)
{
  int sin = sine_table[ angle         & 4095];
  int cos = sine_table[(angle + 1024) & 4095];
  int tx = *x; int ty = *y;
  *x = (cos * tx - sin * ty) >> 12;
  *y = (sin * tx + cos * ty) >> 12;
}

static inline void rot_y(int angle, short *x, short *y, short *z)
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

static inline void project(const p3d* pt, p2d *pr)
{
	int z     = pt->z; // view space
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

p2d     prj_points[8];
surface srfs[4];

// -----------------------------------------------------

int angle;

static inline void transform(short *x, short *y, short *z,short w)
{
  rot_y(angle   , x,y,z);
  rot_z(angle>>1, x,y,z);
  if (w != 0) {
    *z += view_dist;
  }
}

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{
  trsf_surface      tsrfs[8];
	rconvex           rtris[8];
  rconvex_texturing rtexs[8];

  for (int tet = 0; tet < 2 ; ++tet) {
    // animation angle
    angle = tet ? (frame << 4) : -(frame << 5);
    // transform the points
    for (int i = 0; i < 4; ++i) {
      p3d p = points[i];
      transform(&p.x,&p.y,&p.z,1);
      project(&p, &prj_points[(tet<<2) + i]);
    }
    const int *idx =  indices;
    for (int s = (tet<<2) ; s < (tet<<2)+4 ; ++ s) {
      // transform the textured surfaces for rendering at this frame
      surface_transform(&srfs[s-(tet<<2)], &tsrfs[s], transform);
      // prepare texturing
      rconvex_texturing_pre(&tsrfs[s], transform, points + *idx, &rtexs[s]);
      // prepare triangle rasterization
      rconvex_init(&rtris[s], 3,idx, prj_points + (tet<<2));
      idx += 3;
    }
  }

	// before drawing cols wait for previous frame
  wait_all_drawn();

  int bound_s = -1;
  for (int c = 0; c < SCREEN_WIDTH ; ++c) {

    for (int tet = 0; tet < 2 ; ++tet) {
      const int *idx =  indices;
      for (int s = tet<<2 ; s < (tet<<2) + 4 ; ++ s) {

        if (c >= rtris[s].x && c <= rtris[s].last_x && rtexs[s].ded > 0) {

          rconvex_step(&rtris[s],  3,idx, prj_points + (tet<<2));

          int rz = 256;
          int rx = c           - SCREEN_WIDTH/2;
          int ry = rtris[s].ys - SCREEN_HEIGHT/2;

          if (s != bound_s) {
            rconvex_texturing_bind(&rtexs[s]);
            bound_s = s;
          }

          int light = (tsrfs[s].nz>>4);
          if (light > 15) light = 15;

          int dr = surface_setup_span(&tsrfs[s], rx,ry,rz);
          col_send(
            COLDRAW_PLANE_B(rtexs[s].ded,dr),
            COLDRAW_COL(tet == 0 ? 31 : 71 /*texture id*/, rtris[s].ys,rtris[s].ye, light) | PLANE
          );

        }

        // next triangle
        idx += 3;

        // process pending column commands
        col_process();
      }

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
    view_dist = 460;

    // draw screen
    render_frame();

    // next frame
    ++ frame;

  } // while (1)

}

// -----------------------------------------------------

void main()
{
  *LEDS = 1;
  if (core_id()) {
		main_1();
	} else {
		main_0();
	}
}

// -----------------------------------------------------
