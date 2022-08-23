// _____________________________________________________________________________
// |                                                                           |
// |  Quake demo                                                               |
// |  ===================                                                      |
// |                                                                           |
// | @sylefeb 2022-08-15  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#pragma pack(1)

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "sine_table.h"
#ifndef EMUL
#include "api.c"
#else
#include <stdlib.h>
int core_id() { return 0; }
int time()    { return 0; }
#endif
#include "raster.c"
#include "frustum.c"
#include "q.h"

// array of texturing data
#define MAX_RASTER_FACES 512
rconvex_texturing rtexs[MAX_RASTER_FACES];
unsigned char     srf_tex_nfo[MAX_RASTER_FACES * 2];
// array of projected vertices
#define MAX_PRJ_VERTICES 128
p2d prj_vertices_0[MAX_PRJ_VERTICES];
p2d prj_vertices_1[MAX_PRJ_VERTICES];
// raster face ids within frame
volatile int rface_next_id_0;
volatile int rface_next_id_1;
// array of transformed surfaces
trsf_surface trsf_surfaces[n_surfaces];

// #define DEBUG
// ^^^^^^^^^^^^ uncomment to get profiling info over UART

const int z_clip = 256; // near z clipping plane

// frustum
frustum frustum_view; // frustum in view space
frustum frustum_trsf; // frustum transformed in world space

unsigned short vislist[n_max_vislen]; // stores the current vislist

int memchunk[3192]; // a memory chunk to load data in and work with

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

static inline void rot_x(int angle, short *x, short *y, short *z)
{
  int sin = sine_table[angle & 4095];
  int cos = sine_table[(angle + 1024) & 4095];
  int ty = *y; int tz = *z;
  *y = (cos * ty - sin * tz) >> 12;
  *z = (sin * ty + cos * tz) >> 12;
}

// -----------------------------------------------------
// Global state
// -----------------------------------------------------

// frame
int frame = 0;
int tm_frame = 0;
int v_angle_y = 0;

#ifdef DEBUG
unsigned int tm_colprocess;
unsigned int tm_srfspan;
unsigned int num_clipped;
unsigned int num_culled;
unsigned int num_vis;
#endif

// -----------------------------------------------------

static inline void transform(short *x, short *y, short *z, short w)
{
  if (w != 0) { // vertex, else direction
    *x -= view.x; // translate vertices in view space
    *y -= view.y;
    *z -= view.z;
  }
  rot_y(v_angle_y, x, y, z);
#ifndef EMUL
  // rot_z(v_angle_y<<1, x, y, z);
#endif
}

static inline void inv_transform(short *x, short *y, short *z, short w)
{
#ifndef EMUL
  // rot_z(-v_angle_y<<1, x, y, z);
#endif
  rot_y(-v_angle_y, x, y, z);
  if (w != 0) {
    *x += view.x;
    *y += view.y;
    *z += view.z;
  }
}

static inline void project(const p3d* pt, p2d *pr)
{
  // perspective z division
	int z     = (int)pt->z;
  int inv_z = z != 0 ? ((1 << 16) / z) : (1 << 16);
  pr->x = (short)((((int)pt->x * inv_z) >> 8) + SCREEN_WIDTH  / 2);
  pr->y = (short)((((int)pt->y * inv_z) >> 8) + SCREEN_HEIGHT / 2);
}

static inline void unproject(const p2d *pr,short z,p3d* pt)
{
  pt->x = (short)(((((int)pr->x - SCREEN_WIDTH  / 2) << 8) * (int)z) >> 16);
  pt->y = (short)(((((int)pr->y - SCREEN_HEIGHT / 2) << 8) * (int)z) >> 16);
  pt->z = z;
}

// -----------------------------------------------------

typedef struct s_span {
  unsigned char  ys;
  unsigned char  ye;
  unsigned short fid;  // face id
  unsigned short next; // span pos in span_pool
} t_span;

#define MAX_NUM_SPANS 10000
t_span         span_pool   [MAX_NUM_SPANS];
unsigned short span_heads_0[SCREEN_WIDTH]; // span pos in span_pool
unsigned short span_heads_1[SCREEN_WIDTH]; // span pos in span_pool
volatile unsigned short span_alloc_0;
volatile unsigned short span_alloc_1;

#define POLY_MAX_SZ 16 // hope for the best ... (!!)
const int face_indices[POLY_MAX_SZ] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };

// -----------------------------------------------------
#if 1
void render_spans(int c, int ispan)
{
  while (ispan) {
    t_span *span = span_pool + ispan;
    // pixel pos
    int rz = 256;
    int rx = c - SCREEN_WIDTH / 2;
    int ry = span->ys - SCREEN_HEIGHT / 2;
#ifdef DEBUG
    unsigned int tm_ss = time();
#endif
    // bind the surface to the rasterizer
    rconvex_texturing_bind(&rtexs[span->fid]);
    // setup the surface span parameters
    int sid = srf_tex_nfo[(span->fid << 1) + 0];
    int tid = srf_tex_nfo[(span->fid << 1) + 1];
    int dr = surface_setup_span(&trsf_surfaces[sid], rx, ry, rz);
#ifndef EMUL
#ifdef DEBUG
    tm_srfspan += time() - tm_ss;
#endif
    // column drawing
    col_send(
      COLDRAW_PLANE_B(rtexs[span->fid].ded, dr),
      COLDRAW_COL(tid, span->ys, span->ye,
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
      pixelAt(fb, c, j)[0] = 0;
      pixelAt(fb, c, j)[1] = 0;
      pixelAt(fb, c, j)[2] += 16;
    }
#endif
    // next span
    ispan = span->next;
  }

}
#endif
// -----------------------------------------------------

void renderLeaf(int core,const unsigned char *ptr);

volatile int core1_todo;
volatile int core1_done;

void main_1()
{
  core1_todo = 0;
  core1_done = 0;

  while (1) {

    // wait for the order
    while (core1_todo != 1) {}
    core1_todo = 0;
    // render the other leaf
    renderLeaf(1,(const unsigned char*)memchunk);
    // sync
    core1_done = 1;

  }

}

// -----------------------------------------------------

volatile int buf8[2];
volatile int buf12[3];

unsigned short locate_leaf()
{
  unsigned short nid = 0/*root*/;
  while (1) {
    // reached a leaf?
    if (nid & 0x8000) {
      // return leaf index
      return ~nid;
    }
    // get node and plane
    // uses intermediate buffers since length of read has to be multiple of 4
    spiflash_copy(o_bsp_nodes + nid * sizeof(t_my_node), buf8, sizeof(buf8));
    volatile const t_my_node *nd  = (volatile const t_my_node *)buf8;
    spiflash_copy(o_bsp_planes + nd->plane_id * sizeof(t_my_plane), buf12, sizeof(buf12));
    volatile const t_my_plane *pl = (volatile const t_my_plane *)buf12;
    // compute side
    int side = (dot3(view.x,view.y,view.z, pl->nx,pl->ny,pl->nz)>>8) - (pl->dist);
    //                                                       ^^^
    //                                               >>8 for 256 factor
    //printf("view %d,%d,%d nid = %d pl = %d side = %d\n",
    //       view.x, view.y, view.z,
    //       nid, nd->plane_id, side);
    if (side < 0) {
      nid = nd->back;
    } else {
      nid = nd->front;
    }
  }
  return 0;
}

// -----------------------------------------------------

int readLeafVisList(int leaf)
{
  int offset = o_leaves[leaf];
  volatile int start;
  spiflash_copy(offset + 6 * sizeof(short), &start, sizeof(int));
  volatile int len;
  spiflash_copy(offset + 6 * sizeof(short) + sizeof(int), &len, sizeof(int));
  //printf("leaf %d offs %d vis list: %d,%d\n", leaf, offset, start, len);
  // read the entire list in local memory
  spiflash_copy(o_vislist + start * sizeof(short), (volatile int*)vislist, len * sizeof(short) + 4/*ensures we don't miss last bytes if non x4*/);
  // for (int i = 0; i < len; ++i) { printf("%d,", vislist[i]); } printf("\n");
  // now read all bboxes
  volatile unsigned char *dst = (unsigned char *)memchunk;
  for (int i = 0; i < len; ++i) {
    spiflash_copy(o_leaves[vislist[i]], (volatile int *)dst, sizeof(short)*6 );
    dst += sizeof(short) * 6;
  }
  return len;
}

// -----------------------------------------------------

void frustumTest(int len)
{
  int num_culled  = 0;
  int num_visible = 0;
  const aabb *bx = (const aabb *)memchunk;
  for (int i = 0; i < len; ++i) {
    if (!frustum_aabb_overlap(bx, &frustum_trsf)) {
      vislist[i] = 65535; // tag as not visible
      ++num_culled;
    } else {
      ++num_visible;
    }
    ++bx;
  }
  // printf("culled: %d visible: %d\n", num_culled,num_visible);
}

// -----------------------------------------------------

void getLeaf(int leaf,volatile int **p_dst)
{
  int offset = o_leaves[leaf] + sizeof(short) * 6 + sizeof(int) * 2;
  //                            ^^^ bbox            ^^^ vis start,len
  int length = o_leaves[leaf + 1] - offset;
  if (length & 3) { // ensures we get the last bytes
    length += 4;
  }
  // printf("reading leaf %d from %d len %d\n",leaf,offset,length);
  spiflash_copy(offset, *p_dst, length);
  *p_dst += length;
}

// -----------------------------------------------------

void renderLeaf(int core,const unsigned char *ptr)
{
  if (ptr == 0) return;
  // select temporary table
  p2d *prj_vertices;
  if (core == 0) {
    prj_vertices = prj_vertices_0;
  } else {
    prj_vertices = prj_vertices_1;
  }
  // num vertices
  int numv = *(const int*)ptr;
  ptr += sizeof(int);
  // printf("leaf has %d vertices.\n",numv);
  // project vertices
  const p3d *vertices = (const p3d *)ptr;
  ptr += numv * sizeof(p3d);
  for (int v = 0; v < numv; ++v) {
    p3d p = vertices[v];
    transform(&p.x, &p.y, &p.z, 1);
    if (p.z >= z_clip) {
      // project
      project(&p, &prj_vertices[v]);
    } else {
      // tag as clipped
      prj_vertices[v].x = 0x7FFF;
    }
  }
  // rasterize faces
  p3d trsf_vertices[POLY_MAX_SZ];
  p3d face_vertices[POLY_MAX_SZ];
  p2d face_prj_vertices[POLY_MAX_SZ];
  // num faces
  int numf = *(const int*)ptr;
  ptr += sizeof(int);
  // printf("leaf has %d faces.\n", numf);
  const unsigned short *faces   = (const unsigned short *)ptr;
  ptr += numf * sizeof(short) * 4; // 4 shorts per face
  // face indices
  const int *indices = (const int*)ptr;
  // go through faces
  const unsigned short *fptr = faces;
  for (int f = 0; f < numf; ++f) {
    int fc;
    if (core == 0) {
      fc = rface_next_id_0++;
    } else {
      fc = --rface_next_id_1;
    }
    if (rface_next_id_0 >= rface_next_id_1) {
      return;
    }
    int first_idx = *(fptr++);
    int num_idx   = *(fptr++);
    int srfs_idx  = *(fptr++);
    int tex_id    = *(fptr++);
    // check vertices for clipping
    const int *idx = indices + first_idx;
    int n_clipped  = 0;
    int max_x = -2147483647; int max_y = -2147483647;
    int min_x = 2147483647; int min_y = 2147483647;
    for (int v = 0; v < num_idx; ++v) {
      p2d p = prj_vertices[*(idx++)];
      if (p.x == 0x7FFF) {
        ++n_clipped;
      } else {
        if (p.x > max_x) { max_x = p.x; }
        if (p.x < min_x) { min_x = p.x; }
        if (p.y > max_y) { max_y = p.y; }
        if (p.y < min_y) { min_y = p.y; }
      }
    }
    // all clipped => skip
    if (n_clipped == num_idx) {
#ifdef DEBUG
      ++num_culled;
#endif
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // out of bounds => skip
    if (min_x >= SCREEN_WIDTH || max_x <= 0 || min_y >= SCREEN_HEIGHT || max_y <= 0) {
#ifdef DEBUG
      ++num_culled;
#endif
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // prepare texturing info
    rconvex_texturing_pre(&trsf_surfaces[srfs_idx], transform,
      vertices + indices[first_idx], &rtexs[fc]);
    // backface? => skip
    if (rtexs[fc].ded < 0) {
#ifdef DEBUG
      ++num_culled;
#endif
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // surface and texture info
    srf_tex_nfo[(fc << 1) + 0] = srfs_idx;
    srf_tex_nfo[(fc << 1) + 1] = tex_id;
    // clip?
    const int *ptr_indices;
    const p2d *ptr_prj_vertices;
    if (n_clipped == 0) {
      // no clipping required
      ptr_indices = indices + first_idx;
      ptr_prj_vertices = prj_vertices;
    } else {
#ifdef DEBUG
      ++num_clipped;
#endif
      // clip the face
      // -> transform vertices
      const int *idx = indices + first_idx;
      p3d *v_dst = trsf_vertices;
      for (int v = 0; v < POLY_MAX_SZ && v < num_idx; ++v) {
        //            ^^^^^^ encourages unrolling?
        p3d p = vertices[*(idx++)];
        transform(&p.x, &p.y, &p.z, 1);
        *(v_dst++) = p;
      }
      // -> clip
      int n_clipped = 0;
      clip_polygon(z_clip,
        trsf_vertices, num_idx,
        face_vertices, &n_clipped);
#ifdef EMUL
      if (n_clipped >= POLY_MAX_SZ) {
        printf("############# poly clip overflow");
        exit(-1);
      }
#endif
      // -> project clipped vertices
      for (int v = 0; v < n_clipped; ++v) {
        project(&face_vertices[v], &face_prj_vertices[v]);
      }
      // use clipped polygon
      ptr_indices = face_indices;
      ptr_prj_vertices = face_prj_vertices;
      num_idx = n_clipped;
    }
#ifdef DEBUG
    ++num_vis;
#endif
    // rasterize the face into spans
    rconvex rtri;
    rconvex_init(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
    for (int c = rtri.x; c <= rtri.last_x; ++c) {
      rconvex_step(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
      // insert span
      if (span_alloc_0 + (MAX_NUM_SPANS - span_alloc_1) + 1 < MAX_NUM_SPANS && rtri.ys < rtri.ye) {
        // insert span record
        t_span *span;
        if (core_id() == 0) {
          int ispan = ++span_alloc_0; // span 0 unused, tags empty
          span = span_pool + ispan;
          span->next = span_heads_0[c];
          span_heads_0[c] = ispan;
        } else {
          int ispan = --span_alloc_1;
          span = span_pool + ispan;
          span->next = span_heads_1[c];
          span_heads_1[c] = ispan;
        }
        // set span data
        span->ys = (unsigned char)rtri.ys;
        span->ye = (unsigned char)rtri.ye;
        span->fid = fc;
      }
    }
  }
}

// -----------------------------------------------------

void renderLeaves(int len)
{
  int next = 0;
  while (next < len) {
    volatile int *dst = memchunk;
    // read a first leaf
    int *first = 0;
    while (next < len) {
      if (vislist[next] < 65535) {
        first = (int*)dst;
        getLeaf(vislist[next], &dst);
        break;
      }
      ++next;
    }
    ++next;
    // read a second leaf
    int *second = 0;
    while (next < len) {
      if (vislist[next] < 65535) {
        second = (int*)dst;
        getLeaf(vislist[next], &dst);
        break;
      }
      ++next;
    }
    ++next;
    // render leaves
    if (first) {
      core1_done = 0;
      core1_todo = 1; // request core 1 assistance
      renderLeaf(0,(const unsigned char*)second);
      while (core1_done != 1) {} // wait for core 1
    }
    //printf("%d bytes\n", (int)dst - (int)memchunk);
    //printf("%d spans\n", span_alloc_0 + (MAX_NUM_SPANS - span_alloc_1));
    //printf("%d %d rfaces\n", rface_next_id_0, rface_next_id_1);
  }
}

// -----------------------------------------------------

// Draws all screen columns
// runs on core 0, core 1 assists
static inline void render_frame()
{
  // reset rface ids
  rface_next_id_0 = 0;
  rface_next_id_1 = MAX_RASTER_FACES;
  // reset spans
  span_alloc_0 = 0;
  span_alloc_1 = MAX_NUM_SPANS;

#ifdef DEBUG
  unsigned int tm_1 = time();
  num_clipped = 0;
  num_culled = 0;
  num_vis = 0;
#endif

#ifndef EMUL
  // ---- wait for previous frame to be done
  wait_all_drawn();
  // ---- now can access texture memory
#endif

  /// transform frustum in world space
  frustum_transform(&frustum_view, z_clip, inv_transform, unproject, &frustum_trsf);
  /// transform surfaces
  for (int s = 0; s < n_surfaces; ++s) {
    surface_transform(&surfaces[s], &trsf_surfaces[s], transform);
  }
  /// locate current leaf
  unsigned int tm_loc = time();
  unsigned short leaf = locate_leaf();
  unsigned int took   = time() - tm_loc;
  // printf("view %d,%d,%d in leaf %d (took %d cycles)\n", view.x, view.y, view.z, leaf, took);
  /// get visibility list
  int len = readLeafVisList(leaf);
  /// check frustum - aabb
  frustumTest(len);
  /// render visible leaves
  renderLeaves(len);

  //printf("%d spans\n", span_alloc_0 + (MAX_NUM_SPANS - span_alloc_1));

#ifdef DEBUG
  unsigned int tm_2 = time();
#endif

#ifdef DEBUG
  unsigned int tm_3 = time();
  tm_colprocess = 0;
  tm_srfspan = 0;
#endif

  /// render the spans
  for (int c = 0; c < SCREEN_WIDTH; ++c) {

    // go through lists
    int empty = 1;
    for (int l = 0; l < 2; ++l) {
      unsigned short ispan = l == 0 ? span_heads_0[c] : span_heads_1[c];
      empty = empty & !ispan;
      render_spans(c, ispan);
    }

    // background filler
    if (empty) {
      col_send(
        COLDRAW_WALL(Y_MAX, 0, 0),
        COLDRAW_COL(0, 0, 239, 0) | WALL
      );
    }
    // send end of column
    col_send(0, COLDRAW_EOC);

    // clear spans for this column
    span_heads_0[c] = 0;
    span_heads_1[c] = 0;

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
  printf("(cycles) raster %d, wait %d, spans %d, colprocess %d, srfspan %d [tot %d]\n",
     tm_2-tm_1, tm_3-tm_2, tm_4-tm_3, tm_colprocess, tm_srfspan, tm_4-tm_1);
  printf("num spans %d, faces: num vis %d, clipped %d, culled %d\n", span_alloc_0 + (MAX_NUM_SPANS - span_alloc_1),num_vis,num_clipped,num_culled);
#endif

}

// -----------------------------------------------------
// main
// -----------------------------------------------------

void main_0()
{
  unsigned char prev_uart_byte = 0;

  // --------------------------
  // intialize view and frame
  // --------------------------
  frame    = 0;

  // --------------------------
  // init oled
  // --------------------------
#ifndef EMUL
  *LEDS = 0;
  spiflash_init();
  oled_init();
  oled_fullscreen();
#endif

  // prepare the rasterizer
  raster_pre();
  // clear spans
  for (int i=0;i<SCREEN_WIDTH;++i) {
    span_heads_0[i] = 0;
    span_heads_1[i] = 0;
  }

  // prepare the frustum
  frustum_pre(&frustum_view, unproject, z_clip);

  // --------------------------
  // render loop
  // --------------------------
#ifndef EMUL
  view.y     += 16; // move up a bit
#endif
#ifdef EMUL
  v_angle_y += 64;
#else
  v_angle_y = 0;
#endif
  int v_start = view.z - 1000;
  int v_end   = view.z + 2100;
  int v_step  = 1;

  //view.x = 128 << 2;
  //view.y = -208 << 2;
  //view.z = 1216 << 2;

  while (1) {

    tm_frame = time();

    printf("."); // alive

    // draw screen
    render_frame();

    int tm = time();
    int elapsed = tm - tm_frame;
    int speed = (elapsed >> 17);

    prev_uart_byte = uart_byte();
    if (prev_uart_byte == 'a') {
      v_angle_y += speed<<1;
    }
    if (prev_uart_byte == 'd') {
      v_angle_y -= speed<<1;
    }
    if (prev_uart_byte == 'x') {
      view.x += speed;
    }
    if (prev_uart_byte == 'X') {
      view.x -= speed;
    }
    if (prev_uart_byte == 'y') {
      view.y += speed;
    }
    if (prev_uart_byte == 'Y') {
      view.y -= speed;
    }
    if (prev_uart_byte == 'z') {
      view.z += speed;
    }
    if (prev_uart_byte == 'Z') {
      view.z -= speed;
    }

#if 0
#ifndef EMUL
    // walk back and forth
    if (elapsed < 0) { elapsed = 1; }
    int speed   = (elapsed >> 17);
    view.z     += v_step * speed;
    if (view.z > v_end) {
      v_step = 0;
      if (v_angle_y < 2048) {
        v_angle_y += speed<<2;
      } else {
        v_angle_y = 2048;
        view.z    = v_end;
        v_step    = -1;
      }
    } else if (view.z < v_start) {
      v_step = 0;
      if (v_angle_y > 0) {
        v_angle_y -= speed<<2;
      } else {
        v_angle_y = 0;
        view.z = v_start;
        v_step = 1;
      }
    }
#endif
#endif

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
