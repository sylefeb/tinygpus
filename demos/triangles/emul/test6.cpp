// @sylefeb, MIT license
// g++ test1.cpp tga.cpp -o test1

#define EMUL

#include <cstring>
#include <cstdio>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "tga.h"
#include "../../../software/api/raster.c"

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

void raster(t_image_nfo *fb,int n_pts,p2d *pts)
{
  int indices[] = { 0,1,2,3,4,5,6,7,8,9,10 };
  rconvex t;
  rconvex_init(&t, n_pts, indices, pts);
  for (int i = t.x; i <= t.last_x; ++i) {
    rconvex_step(&t, n_pts, indices, pts);
    for (int j = t.ys; j <= t.ye; ++j) {
      pixelAt(fb, i, j)[0] = i;
      pixelAt(fb, i, j)[1] = j;
      pixelAt(fb, i, j)[2]+= 32;
    }
  }
}

/* -------------------------------------------------------- */

int main(int argc,const char **argv)
{
  t_image_nfo *fb = framebuffer(320,240);

#define n_pts 4
  p2d points_a[n_pts] = {
    { 50 - 200, 50},
    {100 - 200, 50},
    {100 - 200,100},
    { 50 - 200,100},
  };
  p2d points_b[n_pts] = {
    {100, 50},
    {150, 50},
    {150,100},
    {100,100},
  };
  p2d points_c[n_pts] = {
    { 50,100},
    {100,100},
    {100,150},
    { 50,150},
  };

  raster_pre();

  raster(fb, n_pts, points_a);
  raster(fb, n_pts, points_b);
  raster(fb, n_pts, points_c);

  SaveTGAFile(SRC_PATH "test6.tga",fb);
}

/* -------------------------------------------------------- */
