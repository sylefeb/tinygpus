// _____________________________________________________________________________
// |                                                                           |
// |  Many triangles demo                                                      |
// |  ===================                                                      |
// |                                                                           |
// | @sylefeb 2022-07-15  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "sine_table.h"
#include "api.c"
#include "raster.c"

//#define DEBUG
// ^^^^^^^^^^^^ uncomment to get profiling info over UART

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

static inline void transform(short *x, short *y, short *z,short w)
{
  rot_z(tr_angle>>1, x,y,z);
  if (w != 0) { // vertex, else direction
    *x += tr_translation.x;
    *y += tr_translation.y;
    *z += tr_translation.z;
    *z += view_dist;
  }
}

static inline void project(const p3d* pt, p2d *pr)
{
	int z     = pt->z;     // view space
	int inv_z = 65536 / z; // NOTE: could precomp in this demo, each triangle
                         //       is at a fixed depth
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

// rasterize multiple instances of the triangle
#define N_TRIS 48
// transformed and projected points
p2d     prj_points[N_PTS];
// transformed texture surface
trsf_surface tsrf;
// texturing info (one per triangle)
rconvex_texturing rtexs[N_TRIS];

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{
  *LEDS = 0;

  // reset spans
  span_alloc = 0;

#ifdef DEBUG
  unsigned int tm_1 = time();
#endif

  // prepare the surface (we reuse the same for all)
  surface_transform(&srf, &tsrf, transform);

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
    // prepare texturing info
    rconvex_texturing_pre(&tsrf,transform,points,&rtexs[t]);
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

#ifdef DEBUG
  unsigned int tm_2 = time();
#endif

	// before drawing cols wait for previous frame
  wait_all_drawn();

#ifdef DEBUG
  unsigned int tm_3 = time();
  unsigned int tm_colprocess = 0;
  unsigned int tm_srfspan = 0;
#endif

  // render the spans
  for (int c = 0; c < SCREEN_WIDTH ; ++c) {
    // go through list
    t_span *span = span_heads[c];
    while (span) {
      // pixel pos
      int rz = 256;
      int rx = c        - SCREEN_WIDTH/2;
      int ry = span->ys - SCREEN_HEIGHT/2;
#ifdef DEBUG
      unsigned int tm_ss = time();
#endif
      // bind the surface to the rasterizer
      rconvex_texturing_bind(&rtexs[span->id]);
      // setup the surface span parameters
      int dr = surface_setup_span(&tsrf, rx,ry,rz);
#ifdef DEBUG
      tm_srfspan += time() - tm_ss;
#endif
      // column drawing
      col_send(
        COLDRAW_PLANE_B(rtexs[span->id].ded,dr),
        COLDRAW_COL(100 + span->id /*texture id*/, span->ys,span->ye,
                    15 /*light*/) | PLANE
      );
      // process pending column commands
#ifdef DEBUG
      unsigned int tm_cp = time();
#endif
      col_process();
#ifdef DEBUG
      tm_colprocess += time() - tm_cp;
#endif
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

    // clear spans for thhis column
    span_heads[c] = 0;

    // process pending column commands
#ifdef DEBUG
      unsigned int tm_cp = time();
#endif
    col_process();
#ifdef DEBUG
      tm_colprocess += time() - tm_cp;
#endif

  }

#ifdef DEBUG
  unsigned int tm_4 = time();
  printf("(cycles) raster %d, wait %d, spans %d, colprocess %d, srfspan %d, tot %d\n",
     tm_2-tm_1, tm_3-tm_2, tm_4-tm_3, tm_colprocess, tm_srfspan, tm_4-tm_1);
  printf("num spans %d\n",span_alloc);
#endif

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
