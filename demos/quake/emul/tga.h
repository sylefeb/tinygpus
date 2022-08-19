#pragma once

typedef unsigned char uchar;
typedef unsigned int  uint;

typedef struct 
{
  uint   width;
  uint   height;
  uchar  depth;
  uchar *pixels;
} t_image_nfo;


t_image_nfo *ReadTGAFile(const char *filename);
bool SaveTGAFile(const char *name,t_image_nfo *img);
