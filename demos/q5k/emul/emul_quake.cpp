// @sylefeb, MIT license
// g++ emul_quake.cpp tga.cpp -o emul_quake

#define EMUL

#include <cstring>
#include <cstdio>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "tga.h"
t_image_nfo *fb;
uchar *pixelAt(t_image_nfo *img, int i, int j);

#include "../q5k.c"

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
  j = ((img->height-1-j)+img->height) % img->height;
  return img->pixels + ( (i+j*img->width)*3 );
}

/* -------------------------------------------------------- */

int main(int argc,const char **argv)
{
  fb = framebuffer(320,240);

  for (int i = 0; i <= 64; ++i) {

    memset(fb->pixels, 0x00, fb->width * fb->height * 3 * sizeof(uchar));

    main_0();

    view.z += 16;

    char str[256];
    sprintf(str, "frame%02d.tga", i);
    SaveTGAFile(str, fb);
  }

}

/* -------------------------------------------------------- */
