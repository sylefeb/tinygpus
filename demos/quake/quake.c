// _____________________________________________________________________________
// |                                                                           |
// |  Quake demo                                                               |
// |  ===================                                                      |
// |                                                                           |
// | @sylefeb 2022-08-15  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "sine_table.h"
#ifndef EMUL
#include "api.c"
#endif
#include "raster.c"
#include "q.h"

// array of projected vertices
p3d prj_vertices[n_vertices];
// array of transformed surfaces
trsf_surface trsf_surfaces[n_surfaces];
// array of textrung data
rconvex_texturing rtexs[n_faces];

// #define DEBUG
// ^^^^^^^^^^^^ uncomment to get profiling info over UART

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

static inline void transform(int *x, int *y, int *z,int w)
{
  if (w != 0) { // vertex, else direction
    *x -= view.x; // translate vertices in view space
    *y -= view.y;
    *z -= view.z;
  }
  rot_y(frame<<2, x, y, z);
}

static inline void project(const p3d* pt, p3d *pr)
{
  // perspective z division
	int z     = pt->z;
	int inv_z = z != 0 ? ((1<<16) / z) : (1<<16);
	pr->x     = ((pt->x * inv_z) >> 8) + SCREEN_WIDTH/2;
	pr->y     = ((pt->y * inv_z) >> 8) + SCREEN_HEIGHT/2;
  pr->z     = z;
}

// -----------------------------------------------------

typedef struct s_span {
  unsigned char  ys;
  unsigned char  ye;
  unsigned short fid; // face id
  struct s_span *next;
} t_span;

#define MAX_NUM_SPANS 10000
t_span  span_pool [MAX_NUM_SPANS];
t_span *span_heads[SCREEN_WIDTH];
int     span_alloc;

#define CLIP_MAX_SZ 16 // hope for the best ... (!!)
p3d clip_vertices[CLIP_MAX_SZ];
p3d clip_prj_vertices[CLIP_MAX_SZ];
int clip_indices[CLIP_MAX_SZ] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };

// -----------------------------------------------------

// draws all screen columns
static inline void render_frame()
{
  const int z_clip = 128; // near z clipping plane

  // reset spans
  span_alloc = 0;

#ifdef DEBUG
  unsigned int tm_1 = time();
#endif

  // transform the vertices
  for (int v = 0; v < n_vertices ; ++v) {
    p3d p = vertices[v];
    transform(&p.x,&p.y,&p.z,1);
    project  (&p, &prj_vertices[v]);
  }
  // transform the surfaces
  for (int s = 0; s < n_surfaces ; ++s) {
    surface_transform(&surfaces[s], &trsf_surfaces[s], transform);
  }
  // rasterize faces
  int *fptr = faces;
  for (int f = 0; f < n_faces ; ++f) {
    int first_idx = *(fptr++);
    int num_idx   = *(fptr++);
    int srfs_idx  = *(fptr++);
    int tex_id    = *(fptr++);
    /*if (f == 256+59) {
      for (int i = first_idx; i < first_idx + num_idx; ++i) {
        printf("%d %d %d,\n", prj_vertices[indices[i]].x, prj_vertices[indices[i]].y, prj_vertices[indices[i]].z);
      }
      printf("\n");
    }*/
    // prepare texturing info
    rconvex_texturing_pre(&trsf_surfaces[srfs_idx], transform,
      vertices + indices[first_idx], &rtexs[f]);
    if (rtexs[f].ded < 0) continue; // backface: skip
    // check for clipping
    int num_clip = 0;
    for (int i = first_idx; i < first_idx + num_idx; ++i) {
      if (prj_vertices[indices[i]].z < z_clip) {
        ++num_clip;
      }
    }
    if (num_clip == num_idx) continue; // fully behind
    int *ptr_indices;
    const p3d *ptr_prj_vertices;
    if (num_clip > 0) {
      // clip the face
      int n_clipped = 0;
      clip_polygon(transform, num_idx, indices + first_idx, vertices,
                   z_clip, clip_vertices, &n_clipped);
      // project clipped vertices
      for (int v = 0; v < n_clipped; ++v) {
        project(&clip_vertices[v], &clip_prj_vertices[v]);
      }
      /*if (f == 256 + 36) {
        for (int i = 0; i < n_clipped; ++i) {
          printf("%d %d %d,\n", clip_prj_vertices[i].x, clip_prj_vertices[i].y, clip_prj_vertices[i].z);
        }
        printf("\n");
      }*/
      ptr_indices      = clip_indices;
      ptr_prj_vertices = clip_prj_vertices;
      num_idx = n_clipped;
    } else {
      ptr_indices      = indices + first_idx;
      ptr_prj_vertices = prj_vertices;
    }
    // rasterize the face into spans
    rconvex rtri;
    rconvex_init(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
    for (int c = rtri.x ; c <= rtri.last_x ; ++c) {
      rconvex_step(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
      // insert span
      if (span_alloc < MAX_NUM_SPANS && rtri.ys < rtri.ye) {
        int ispan     = span_alloc ++;
        t_span *span  = span_pool + ispan;
        span->ys      = rtri.ys;
        span->ye      = rtri.ye;
        span->fid     = f;
        // if (span->fid == 256+36) { printf("%d -> %d\n", span->ys, span->ye); }
        span->next    = span_heads[c];
        span_heads[c] = span;
      }
    }
  }
#ifdef EMUL
  printf("%d spans\n", span_alloc);
#endif

#ifdef DEBUG
  unsigned int tm_2 = time();
#endif

#ifndef EMUL
  // before drawing cols wait for previous frame
  wait_all_drawn();
#endif

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
      rconvex_texturing_bind(&rtexs[span->fid]);
      // setup the surface span parameters
      int sid = faces[(span->fid << 2) + 2];
      int tid = faces[(span->fid << 2) + 3];
      int dr  = surface_setup_span(&trsf_surfaces[sid], rx,ry,rz);
      //printf("ded:%d dr:%d\n",rtexs[span->fid].ded,dr);
#ifndef EMUL
#ifdef DEBUG
      tm_srfspan += time() - tm_ss;
#endif
      // column drawing
      col_send(
        COLDRAW_PLANE_B(rtexs[span->fid].ded,dr),
        COLDRAW_COL(tid, span->ys,span->ye,
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
#else
      for (int j = span->ys; j <= span->ye; ++j) {
        pixelAt(fb, c, j)[0] = span->fid&255;
        pixelAt(fb, c, j)[1] = (span->fid>>8)&255;
        pixelAt(fb, c, j)[2] = span->fid>>16;
      }
#endif
      // next span
      span = span->next;
    }

#ifndef EMUL
    // background filler
    col_send(
      COLDRAW_WALL(Y_MAX,0,0),
      COLDRAW_COL(0, 0,239, 0) | WALL
    );
    // send end of column
    col_send(0, COLDRAW_EOC);
#endif
    // clear spans for thhis column
    span_heads[c] = 0;

#ifndef EMUL
    // process pending column commands
#ifdef DEBUG
      unsigned int tm_cp = time();
#endif
    col_process();
#ifdef DEBUG
      tm_colprocess += time() - tm_cp;
#endif
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
  // --------------------------
  // intialize view and frame
  // --------------------------
  frame    = 0;

  // --------------------------
  // init oled
  // --------------------------
#ifndef EMUL
  *LEDS = 0;
  oled_init();
  oled_fullscreen();
#endif

  // prepare the rasterizer
  raster_pre();
  // clear spans
  for (int i=0;i<SCREEN_WIDTH;++i) {
    span_heads[i] = 0;
  }

  // --------------------------
  // render loop
  // --------------------------
  int v_base = view.z;

#ifndef EMUL
  while (1)
#endif
  {

    // draw screen
    render_frame();

    view.z += 16;
    if ((frame & 63) == 0) view.z = v_base;

    // next frame
    ++ frame;

  } // while (1)

}

// -----------------------------------------------------

#ifndef EMUL
void main()
{
  if (core_id()) {
		main_1();
	} else {
		main_0();
	}
}
#endif

// -----------------------------------------------------
