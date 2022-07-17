// @sylefeb, MIT license
// g++ test2.cpp tga.cpp -o test2

#define EMUL

#include <cstring>
#include <cstdio>

#include "tga.h"
#include "../../../software/api/sine_table.h"
#include "../../../software/api/raster.c"

t_image_nfo *fb = NULL;

/* -------------------------------------------------------- */

t_image_nfo *framebuffer(uint w,uint h)
{
  t_image_nfo *img = new t_image_nfo;
  img->width  = w;
  img->height = h;
  img->depth  = 24; // RGB x 8 bits
  img->pixels = new uchar[ w*h*3*sizeof(uchar) ];
  // initialize to gray
  memset(img->pixels,0x80,w*h*3*sizeof(uchar));
  return img;
}

/* -------------------------------------------------------- */

uchar *pixelAt(t_image_nfo *img,int i,int j)
{
  i = (i+img->width)  % img->width;
  j = (j+img->height) % img->height;
  return img->pixels + ( (i+j*img->width)*3 );
}

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
int      frame;

// -----------------------------------------------------

#define N_PTS 3

const p3d points[N_PTS] = {
  { 64,   0,   0},
  {   0, 64,   0},
  {-64,   0,   0},
};

const int indices[3] = {
  0,1,2,
};

p3d     trsf_points[N_PTS];

surface srf;

int     angle;

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

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define MAX_NUM_SPANS 1024
t_span  span_pool [MAX_NUM_SPANS];
t_span *span_heads[SCREEN_WIDTH];
int     span_alloc;

// -----------------------------------------------------

void render_frame()
{
  // animation
  angle = frame << 6;

	// transform the points
 	for (int i = 0; i < N_PTS; ++i) {
    p3d p = points[i];
    transform(&p.x,&p.y,&p.z);
		project(&p, &trsf_points[i]);
 	}

  // texturing surface transform
  trsf_surface tsrf;
  surface_transform(&srf, &tsrf, 600, transform, points);
  // NOTE: trsf.ded < 0 if back facing

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

      for (int j = span->ys ; j < span->ye ; ++j) {
        pixelAt(fb, c,j)[0] = c;
        pixelAt(fb, c,j)[1] = j;
        pixelAt(fb, c,j)[2] = 0;
      }

      // next span
      span = span->next;
    }

  }
}

// -----------------------------------------------------

void clear_frame()
{
  memset(fb->pixels,0x80,SCREEN_WIDTH*SCREEN_HEIGHT*3*sizeof(uchar));
}

// -----------------------------------------------------

int main(int argc,const char **argv)
{
  fb = framebuffer(320,240);

  raster_pre();

  for (int f = 0; f < 128; ++f) {
    frame = f;
    clear_frame();
    render_frame();
    char str[512];
    sprintf(str,"test2_%03d.tga",f);
    SaveTGAFile(str,fb);
  }
}

/* -------------------------------------------------------- */
