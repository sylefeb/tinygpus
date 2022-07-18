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

// Define three vertices
const p3d points[N_PTS] = {
  { 120,   0,   0},
  {   0, 200,   0},
  {-120,   0,   0},
};
// Define a single triangle from the vertices
const int indices[3] = { 0,1,2 };
// Texturing surface definition
surface srf;

// -----------------------------------------------------

const int view_dist = 700;

// Global parameters of transformation
int tr_angle;       // rotation angle
p3d tr_translation; // translation

static inline void transform(int *x, int *y, int *z,int w)
{
  rot_z(tr_angle>>1, x,y,z);
  if (w != 0) { // vertex, else direction
    *x += tr_translation.x;
    *y += tr_translation.y;
    *z += tr_translation.z;
    *z += view_dist;
  }
}

static inline void project(const p3d* pt, p3d *pr)
{
	int z     = pt->z; // view space
	int inv_z = 65536 / z;
	pr->x = ((pt->x * inv_z) >> 8) + SCREEN_WIDTH/2;
	pr->y = ((pt->y * inv_z) >> 8) + SCREEN_HEIGHT/2;
}

// -----------------------------------------------------

typedef struct s_span {
  unsigned char ys;
  unsigned char ye;
  unsigned char id; // triangle id
  struct s_span *next;
} t_span;

#define MAX_NUM_SPANS 4096
t_span  span_pool [MAX_NUM_SPANS];
t_span *span_heads[SCREEN_WIDTH];
int     span_alloc;

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{

  // reset spans
  span_alloc = 0;

  // rasterize multiple instances of the triangle
  #define N_TRIS 48
  // transformed and projected points
  p3d     prj_points[N_PTS];
  // transformed texture surfaces (one per triangle)
  trsf_surface tsrf[N_TRIS];
  for (int t = 0; t < N_TRIS ; ++t) {
    // animation
    tr_angle         = (frame << 7) + (t << 9);
    tr_translation.x = sine_table[ ((frame << 6) + (t << 8)) & 4095] >> 3;
    tr_translation.y = sine_table[ ((frame << 5) + (t << 9)) & 4095] >> 4;
    tr_translation.z = t<<6;
    // transform the points
    for (int i = 0; i < N_PTS; ++i) {
      p3d p = points[i];
      transform(&p.x,&p.y,&p.z,1);
      project(&p, &prj_points[i]);
    }
    // prepare the surface
    surface_transform(&srf, &tsrf[t], transform, points);
    // rasterize triangle into spans
    rconvex rtri;
    rconvex_init(&rtri, 3,indices, prj_points);
    for (int c = rtri.x ; c <= rtri.last_x ; ++c) {
      rconvex_step(&rtri, 3,indices, prj_points);
      // insert span
      if (span_alloc < MAX_NUM_SPANS) {
        int ispan     = span_alloc ++;
        t_span *span  = span_pool + ispan;
        span->ys      = rtri.ys;
        span->ye      = rtri.ye;
        span->id      = t;
        span->next    = span_heads[c];
        span_heads[c] = span;
      }
    }
  }

	// before drawing cols wait for previous frame
  wait_all_drawn();

  // render the spans
  int last_bound_srf = -1;
  for (int c = 0; c < SCREEN_WIDTH ; ++c) {
    // go through list
    t_span *span = span_heads[c];
    while (span) {
      // pixel pos
      int rz = 256;
      int rx = c        - SCREEN_WIDTH/2;
      int ry = span->ys - SCREEN_HEIGHT/2;
      // bind the surface to the rasterizer if necessary
      if (last_bound_srf != span->id) {
        surface_bind(&tsrf[span->id]);
        last_bound_srf = span->id;
      }
      // sets the surface span parameters
      surface_set_span(&tsrf[span->id], rx,ry,rz);
      // column drawing
      col_send(
        COLDRAW_PLANE_B(tsrf[span->id].ded,tsrf[span->id].dr),
        COLDRAW_COL(82 + span->id /*texture id*/, span->ys,span->ye,
                    15 /*light*/) | PLANE
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

    // clear span
    span_heads[c] = 0;

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

  // prepare the rasterizer
  raster_pre();
  // clear spans
  for (int i=0;i<SCREEN_WIDTH;++i) {
    span_heads[i] = 0;
  }

  // --------------------------
  // prepare the texturing surfaces
  // --------------------------
  // in this demo we use a single surface for all triangles
  surface_pre(&srf, *indices,*(indices+1),*(indices+2), points);

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
