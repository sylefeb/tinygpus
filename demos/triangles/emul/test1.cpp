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

/* -------------------------------------------------------- */

int main(int argc,const char **argv)
{
  t_image_nfo *fb = framebuffer(320,240);

  p3d points[4] = {
    {50,10,0},
    {10,150,0},
    {300,200,0},
    {250,50,0},
  };

  raster_pre();

  int indices[]={0,3,2,1};

  fprintf(stderr,"sizeof rconvex: %d\n",sizeof(rconvex));

  rconvex t;
	rconvex_init(&t, 4,indices,  points);

  for (int i=t.x;i<=t.last_x;++i) {

    rconvex_step(&t, 4,indices,  points);

    for (int j = t.ys ; j < t.ye ; ++j) {
      pixelAt(fb, i,j)[0] = i;
      pixelAt(fb, i,j)[1] = j;
      pixelAt(fb, i,j)[2] = 0;
    }

  }

  SaveTGAFile("test1.tga",fb);
}

/* -------------------------------------------------------- */
