// _____________________________________________________________________________
// |                                                                           |
// |  Many triangles demo                                                      |
// |  ===================                                                      |
// |                                                                           |
// |  work in progress                                                         |
// |                                                                           |
// | @sylefeb 2022-07-15  licence: GPL v3, see full text in repo               |
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

#define N_PTS 3

const p3d points[N_PTS] = {
  {  64,   0,   0},
  {   0, 64,   0},
  { -64,   0,   0},
};

const int indices[3] = {
  0,1,2,
};

p3d     trsf_points[N_PTS];

surface srf;

// -----------------------------------------------------

int angle;

static inline void transform(int *x, int *y, int *z)
{
  rot_z(angle>>1, x,y,z);
}

static inline void project(const p3d* pt, p3d *pr)
{
	pr->x = pt->x + 160;
	pr->y = pt->y + 100;
}

// -----------------------------------------------------

typedef struct s_span {
  int ys;
  int ye;
  struct s_span *next;
} t_span;

#define MAX_NUM_SPANS 1024
t_span  span_pool [MAX_NUM_SPANS];
t_span *span_heads[SCREEN_WIDTH];
int     span_alloc;

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{
  // animation
  angle = frame << 6;

	// transform the points
 	for (int i = 0; i < N_PTS; ++i) {
    p3d p = points[i];
    transform(&p.x,&p.y,&p.z);
		project(&p, &trsf_points[i]);
 	}

  // reset spans
  span_alloc = 0;
  for (int i=0;i<SCREEN_WIDTH;++i) {
    span_heads[i] = 0; // NOTE: could be cleared at startup and
  }                    //       then after each render

	// rasterize triangle into spans
  rconvex rtri;
	rconvex_init(&rtri, 3,indices, trsf_points);
  for (int c = rtri.x ; c <= rtri.last_x ; ++c) {
    rconvex_step(&rtri, 3,indices, trsf_points);
    // insert span
    if (span_alloc < MAX_NUM_SPANS) {
      int ispan = span_alloc ++;
      t_span *span = span_pool + ispan;
      span->ys = rtri.ys;
      span->ye = rtri.ye;
      span->next    = span_heads[c];
      span_heads[c] = span;
    }
  }

	// before drawing cols wait for previous frame
  wait_all_drawn();

  // texturing surface transform
  trsf_surface tsrf;
  surface_transform(&srf, &tsrf, 700, transform, points);
  // NOTE: trsf.ded < 0 if back facing
  // bind the surface to the rasterizer
  surface_bind(&tsrf);

  // render the spans
  for (int c = 0; c < SCREEN_WIDTH ; ++c) {
    // go through list
    t_span *span = span_heads[c];
    while (span) {

      int rz = 256;
      int rx = c        - SCREEN_WIDTH/2;
      int ry = span->ys - SCREEN_HEIGHT/2;

      surface_set_span(&tsrf, rx,ry,rz);

      col_send(
        COLDRAW_PLANE_B(tsrf.ded,tsrf.dr),
        COLDRAW_COL(125/*texture id*/, span->ys,span->ye, 15 /*light*/) | PLANE
      );

      // process pending column commands
      col_process();
      // next span
      span = span->next;
    }

    // background filler
    col_send(
      COLDRAW_WALL(Y_MAX,0,0),
      COLDRAW_COL(0, 0,239, 0) | WALL
    );

    // send end of column
    col_send(0, COLDRAW_EOC);

    // process pending column commands
    col_process();

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
  surface_pre(&srf, *indices,*(indices+1),*(indices+2), points);

  // prepare the rasterizer
  raster_pre();

  // --------------------------
  // render loop
  // --------------------------
  while (1) {

    // printf("frame %d\n",frame);

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
