// @sylefeb, MIT license
// g++ test2.cpp tga.cpp -o test2

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

/* -------------------------------------------------------- */

int main(int argc,const char **argv)
{
  t_image_nfo *fb = framebuffer(SCREEN_WIDTH,SCREEN_HEIGHT);

  p3d points[3] = {
    {-50,10,0},
    {160,150,0},
    {340,10,0},
  };

  raster_pre();

  int indices[]={0,2,1};

  rconvex t;
	rconvex_init(&t, 3,indices,  points);

  if (t.x < 0) {
    printf("error: starts out of screen (%d)",t.x);
  }
  if (t.last_x >= SCREEN_WIDTH) {
    printf("error: ends out of screen (%d)",t.last_x);
  }

  for (int i=t.x;i<=t.last_x;++i) {

    printf("%d] %d->%d\n",i,t.ys,t.ye);

    rconvex_step(&t, 3,indices,  points);

    for (int j = t.ys ; j < t.ye ; ++j) {
      pixelAt(fb, i,j)[0] = i;
      pixelAt(fb, i,j)[1] = j;
      pixelAt(fb, i,j)[2] = 0;
    }

  }

  SaveTGAFile("test2.tga",fb);
}

/* -------------------------------------------------------- */
