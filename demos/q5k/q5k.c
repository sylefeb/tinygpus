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

#define DEBUG
// ^^^^^^^^^^^^ uncomment to get profiling info over UART

const int z_clip = 64; // near z clipping plane

// -----------------------------------------------------

typedef struct {
  rconvex_texturing rtex;
  unsigned char nrm_id;
  unsigned char tvc_id;
  unsigned char tex_id;
  unsigned char lmap_id;
  int lu_offs,lv_offs;
} t_qrtexs;

// array of texturing data
#define MAX_RASTER_FACES 500
t_qrtexs          rtexs[MAX_RASTER_FACES];

// -----------------------------------------------------

// array of projected vertices
p2d prj_vertices_0[n_max_verts];
p2d prj_vertices_1[n_max_verts];

// raster face ids within frame
volatile int rface_next_id_0;
volatile int rface_next_id_1;

// array of transformed normals
p3d       trsf_normals[n_normals];
// array of transformed texturing vectors
t_texvecs trsf_texvecs[n_texvecs];

// frustum
frustum frustum_view; // frustum in view space
frustum frustum_trsf; // frustum transformed in world space

unsigned short vislist[n_max_vislen]; // stores the current vislist

#define memchunk_size (8+(n_max_leaf_size>>1))
int memchunk[memchunk_size]; // a memory chunk to load data in and work with
//              ^^^^ we have to hold two leafs (core0/core1)

// -----------------------------------------------------

typedef struct s_span {
  unsigned char  ys;
  unsigned char  ye;
  unsigned short fid;  // face id
  unsigned short next; // span pos in span_pool
} t_span;

#define MAX_NUM_SPANS 11000
t_span         span_pool   [MAX_NUM_SPANS];
unsigned short span_heads_0[SCREEN_WIDTH]; // span pos in span_pool
unsigned short span_heads_1[SCREEN_WIDTH]; // span pos in span_pool
volatile unsigned short span_alloc_0;
volatile unsigned short span_alloc_1;

// -----------------------------------------------------

#define POLY_MAX_SZ 20 // hope for the best ... (!!)
const int face_indices[POLY_MAX_SZ] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };

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
int frame;
int tm_frame;
int v_angle_y;
int v_angle_x;

#ifdef DEBUG
unsigned int tm_colprocess;
unsigned int tm_srfspan;
unsigned int tm_api;
unsigned int num_clipped;
#endif

// -----------------------------------------------------

static inline void rconvex_texturing_with_lmap(
                                     const p3d *n,const p3d *u,const p3d *v,
                                     int d_u,int d_v,
                                     int l_u,int l_v,
                                     f_transform trsf,
                                     const p3d *p_ref_uv,
                                     const p3d *p_ref,
                                     t_qrtexs *qrtex)
{
  p3d trp0 = {0,0,0}; // ref point for texturing (origin)
  trsf(&trp0.x,&trp0.y,&trp0.z,1);
  // uv translation: translate so that ref point coordinates map on (0,0)
  qrtex->rtex.u_offs = dot3( trp0.x,trp0.y,trp0.z, u->x,u->y,u->z );
  qrtex->rtex.v_offs = dot3( trp0.x,trp0.y,trp0.z, v->x,v->y,v->z );
  // plane distance
  trp0 = *p_ref; // transform reference point for surface
  trsf(&trp0.x, &trp0.y, &trp0.z, 1);
  qrtex->rtex.ded    = dot3( trp0.x,trp0.y,trp0.z, n->x,n->y,n->z ) >> 8;
  // NOTE: ded < 0 ==> backface surface
  // texturing offset
  if (qrtex->rtex.ded > 0) {
    qrtex->rtex.u_offs = - qrtex->rtex.u_offs;
    qrtex->rtex.v_offs = - qrtex->rtex.v_offs;
  }
  qrtex->rtex.u_offs += d_u << 8;
  qrtex->rtex.v_offs += d_v << 8;
  // light map offset
  trp0 = *p_ref_uv; // transform reference point for lightmap
  trsf(&trp0.x, &trp0.y, &trp0.z, 1);
  qrtex->lu_offs = dot3( trp0.x,trp0.y,trp0.z, u->x,u->y,u->z );
  qrtex->lv_offs = dot3( trp0.x,trp0.y,trp0.z, v->x,v->y,v->z );
  if (qrtex->rtex.ded > 0) {
    qrtex->lu_offs = - qrtex->lu_offs;
    qrtex->lv_offs = - qrtex->lv_offs;
  }
  qrtex->lu_offs += l_u << 8;
  qrtex->lv_offs += l_v << 8;
}

// -----------------------------------------------------

static inline void rconvex_texturing_bind_lightmap(const t_qrtexs *qrtex)
{
#ifndef EMUL
  col_send(
    PARAMETER_UV_OFFSET(qrtex->lv_offs),
    PARAMETER_UV_OFFSET_EX(qrtex->lu_offs) | PARAMETER | LIGHTMAP_EN
  );
#endif
}

// -----------------------------------------------------

static inline void transform(short *x, short *y, short *z, short w)
{
  if (w != 0) { // vertex, else direction
    *x -= view.x; // translate vertices in view space
    *y -= view.y;
    *z -= view.z;
  }
  rot_y(v_angle_y, x, y, z);
  rot_x(v_angle_x, x, y, z);
}

static inline void inv_transform(short *x, short *y, short *z, short w)
{
  rot_x(-v_angle_x, x, y, z);
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

void render_spans(int c, int ispan)
{
  // pixel pos (x,z)
  int rz = 256;
  int rx = c - SCREEN_WIDTH / 2;
  // go through span list
  while (ispan) {
    t_span *span = span_pool + ispan;
    // pixel pos (y)
    int ry = span->ys - SCREEN_HEIGHT / 2;

#ifdef DEBUG
    unsigned int tm_ss = time();
#endif

    // surface_setup_span_nuv
    int nid = rtexs[span->fid].nrm_id;
    int sid = rtexs[span->fid].tvc_id;
    const p3d *n = &trsf_normals[nid];
    const p3d *u = &trsf_texvecs[sid].vecS;
    const p3d *v = &trsf_texvecs[sid].vecT;
    int dr = dot3( rx,ry,rz, n->x,n->y,n->z )>>8;
    int du = dot3( rx,ry,rz, u->x,u->y,u->z )>>8;
    int dv = dot3( rx,ry,rz, v->x,v->y,v->z )>>8;
    // texture ids
    int tid = rtexs[span->fid].tex_id;
    int lid = rtexs[span->fid].lmap_id;

#ifdef DEBUG
    unsigned int tm_ap = time();
    tm_srfspan += tm_ap - tm_ss;
#endif

#if 0
    // bind the surface to the rasterizer
    col_send(
      PARAMETER_PLANE_A(n->y,u->y,v->y),
      PARAMETER_PLANE_A_EX(du,dv) | PARAMETER
    );
    // rconvex_texturing_bind
    col_send(
      PARAMETER_UV_OFFSET(rtexs[span->fid].rtex.v_offs),
      PARAMETER_UV_OFFSET_EX(rtexs[span->fid].rtex.u_offs) | PARAMETER
    );
    // column drawing
    col_send(
      COLDRAW_PLANE_B(rtexs[span->fid].rtex.ded, dr),
      COLDRAW_COL(tid, span->ys, span->ye, 15) | PLANE
    );
    // light map
    col_send(
      PARAMETER_PLANE_A(n->y,u->y,v->y),
      PARAMETER_PLANE_A_EX(du,dv) | PARAMETER
    );
    // rconvex_texturing_bind_lightmap
    col_send(
      PARAMETER_UV_OFFSET(rtexs[span->fid].lv_offs),
      PARAMETER_UV_OFFSET_EX(rtexs[span->fid].lu_offs) | PARAMETER | LIGHTMAP_EN
    );
    // column drawing
    col_send(
      COLDRAW_PLANE_B(rtexs[span->fid].rtex.ded, dr),
      COLDRAW_COL(lid, span->ys, span->ye, 15) | PLANE
    );
#else
    *PARAMETER_PLANE_A_ny     = n->y;
    *PARAMETER_PLANE_A_uy     = u->y;
    *PARAMETER_PLANE_A_vy     = v->y;
    *PARAMETER_PLANE_A_EX_du  = du;
    *PARAMETER_PLANE_A_EX_dv  = dv;
    *PARAMETER_UV_OFFSET_v    = rtexs[span->fid].rtex.v_offs;
    *PARAMETER_UV_OFFSET_EX_u = rtexs[span->fid].rtex.u_offs;
    *PARAMETER_UV_OFFSET_EX_lmap = 0;
    *COLDRAW_PLANE_B_ded      = rtexs[span->fid].rtex.ded;
    *COLDRAW_PLANE_B_dr       = dr;
    *COLDRAW_COL_texid        = tid;
    // *COLDRAW_COL_light   = 15;
    *COLDRAW_COL_start        = span->ys;
    *COLDRAW_COL_end          = span->ye;
    //
    *PARAMETER_PLANE_A_ny = n->y;
    *PARAMETER_PLANE_A_uy = u->y;
    *PARAMETER_PLANE_A_vy = v->y;
    *PARAMETER_PLANE_A_EX_du = du;
    *PARAMETER_PLANE_A_EX_dv = dv;
    *PARAMETER_UV_OFFSET_v    = rtexs[span->fid].lv_offs;
    *PARAMETER_UV_OFFSET_EX_u = rtexs[span->fid].lu_offs;
    *PARAMETER_UV_OFFSET_EX_lmap = 1;
    *COLDRAW_PLANE_B_ded = rtexs[span->fid].rtex.ded;
    *COLDRAW_PLANE_B_dr  = dr;
    *COLDRAW_COL_texid   = lid;
    *COLDRAW_COL_start   = span->ys;
    *COLDRAW_COL_end     = span->ye;
#endif
    // process pending column commands
#ifdef DEBUG
    unsigned int tm_cp = time();
    tm_api += tm_cp - tm_ap;
#endif
    col_process();
#ifdef DEBUG
    tm_colprocess += time() - tm_cp;
#endif
    // next span
    ispan = span->next;
  }

}

// -----------------------------------------------------

volatile int vfc_len;

void renderLeaf(int core,const unsigned char *ptr);
void frustumTest(int first,int last);

volatile int core1_todo;
volatile int core1_done;

void main_1()
{
  core1_todo = 0;
  core1_done = 0;

  while (1) {

    // wait for the order
    while (core1_todo == 0) {}
    int todo = core1_todo;
    core1_todo = 0;
    switch (todo) {
      case 1:
        // render the other leaf
        renderLeaf(1,(const unsigned char*)memchunk);
        break;
      case 2:
        // frustum vis on other half
        frustumTest((vfc_len>>1)+1,vfc_len-1);
        break;
    }
    // sync
    core1_done = 1;

  }

}

// -----------------------------------------------------

unsigned short locate_leaf()
{
  volatile int buf20[5];
  volatile int buf12[3];
  unsigned short nid = 0/*root*/;
  while (1) {
    // reached a leaf?
    if (nid & 0x8000) {
      // return leaf index
      return ~nid;
    }
    // get node and plane
    // uses intermediate buffers since length of read has to be multiple of 4
    spiflash_copy(o_bsp_nodes + nid * sizeof(t_my_node), buf20, sizeof(buf20));
    volatile const t_my_node *nd  = (volatile const t_my_node *)buf20;
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

static inline int leafOffset(int leaf)
{
  volatile int offset;
  spiflash_copy(o_leaf_offsets + leaf*sizeof(int), &offset, sizeof(int));
  return offset;
}

// -----------------------------------------------------

int readLeafVisList(int leaf)
{
  int offset = leafOffset(leaf);
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
    spiflash_copy(leafOffset(vislist[i]), (volatile int *)dst, sizeof(short)*6 );
    dst += sizeof(short) * 6;
  }
  return len;
}

// -----------------------------------------------------

void frustumTest(int first,int last)
{
  int num_culled  = 0;
  int num_visible = 0;
  const aabb *bx = (const aabb *)memchunk + first;
  for (int i = first; i <= last; ++i) {
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
  int offset = leafOffset(leaf) + sizeof(short) * 6 + sizeof(int) * 2;
  //                            ^^^ bbox            ^^^ vis start,len
  int length = leafOffset(leaf + 1) - offset;
  if (length & 3) { // ensures we get the last bytes
    length += 4;
  }
  // printf("4 reading leaf %d from %d len %d\n",leaf,offset,length);
  spiflash_copy(offset, *p_dst, length);
  *p_dst += (length>>2); // int pointer, hence >>2
}

// -----------------------------------------------------

void renderLeaf(int core,const unsigned char *ptr)
{
  if (ptr == 0) {
    return;
  }
  // select temporary table
  p2d *prj_vertices;
  if (core == 0) { prj_vertices = prj_vertices_0;
  } else         { prj_vertices = prj_vertices_1; }
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
  const unsigned short *faces = (const unsigned short *)ptr;
  ptr += numf * sizeof(short) * 10; // 10 shorts per face
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
      printf("#F\n");
      return;
    }
    unsigned short first_idx = *(fptr++);
    unsigned short num_idx   = *(fptr++);
    unsigned short nrm_id    = *(fptr++);
    unsigned short tvc_id    = *(fptr++);
    unsigned short tex_id    = *(fptr++);
    unsigned short lmap_id   = *(fptr++);
    unsigned short lmap_uv   = *(fptr++);
    p3d            lmap_pref;
    lmap_pref.x              = *(fptr++);
    lmap_pref.y              = *(fptr++);
    lmap_pref.z              = *(fptr++);
    // check vertices for clipping
    const int *idx = indices + first_idx;
    int n_clipped  = 0;
    int max_x      = -2147483647; int max_y = -2147483647;
    int min_x      = 2147483647;  int min_y = 2147483647;
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
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // out of bounds => skip
    if (min_x >= SCREEN_WIDTH || max_x <= 0 || min_y >= SCREEN_HEIGHT || max_y <= 0) {
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // prepare texturing info
    char upos = lmap_uv & 255;
    char vpos = lmap_uv >> 8;
    // p3d o = {0,0,0};
    rconvex_texturing_with_lmap(
      &trsf_normals[nrm_id],
      &trsf_texvecs[tvc_id].vecS, &trsf_texvecs[tvc_id].vecT,
      trsf_texvecs[tvc_id].distS, trsf_texvecs[tvc_id].distT,
      ((int)upos) << 6, ((int)vpos) << 6,
      transform,
      &lmap_pref,
      vertices + indices[first_idx],
      &rtexs[fc]);
    // backface? => skip
    if (rtexs[fc].rtex.ded < 0) {
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
    // surface and texture info
    rtexs[fc].nrm_id  = nrm_id;
    rtexs[fc].tvc_id  = tvc_id;
    rtexs[fc].tex_id  = tex_id;
    rtexs[fc].lmap_id = lmap_id;
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
      for (int v = 0; v < num_idx; ++v) {
        p3d p = vertices[*(idx++)];
        transform(&p.x, &p.y, &p.z, 1);
        *(v_dst++) = p;
      }
      // -> clip
      int n_clipped = 0;
      clip_polygon(z_clip, trsf_vertices, num_idx,
                           face_vertices, &n_clipped);
      if (n_clipped >= POLY_MAX_SZ) {
        printf("#P\n");
      }
      // -> project clipped vertices
      for (int v = 0; v < n_clipped; ++v) {
        project(&face_vertices[v], &face_prj_vertices[v]);
      }
      // use clipped polygon
      ptr_indices = face_indices;
      ptr_prj_vertices = face_prj_vertices;
      num_idx = n_clipped;
    }
    // rasterize the face into spans
    rconvex rtri;
    int ok = rconvex_init(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
    if (!ok) {
      // free up slot
      if (core == 0) { --rface_next_id_0; } else { ++rface_next_id_1; }
      continue;
    }
#if 1
    if (core_id() == 0) {
      if (span_alloc_0 + (rtri.last_x - rtri.x + 1) >= span_alloc_1) {
        return;
      }
      for (int c = rtri.x; c <= rtri.last_x; ++c) {
        rconvex_step(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
        //if (rtri.ys < rtri.ye) {
        /// TODO: ^^^^^^^^ clearly there are still some reverted spans, ^
        ///       but filtering is too expensive
        // insert span record
        t_span *span;
        int ispan = ++span_alloc_0; // span 0 unused, tags empty
        span = span_pool + ispan;
        span->next = span_heads_0[c];
        span_heads_0[c] = ispan;
        // set span data
        span->ys = (unsigned char)rtri.ys;
        span->ye = (unsigned char)rtri.ye;
        span->fid = fc;
        //}
      }
    } else {
      if (span_alloc_1 - (rtri.last_x - rtri.x + 1) <= span_alloc_0) {
        return;
      }
      for (int c = rtri.x; c <= rtri.last_x; ++c) {
        rconvex_step(&rtri, num_idx, ptr_indices, ptr_prj_vertices);
        //if (rtri.ys < rtri.ye) {
        // insert span record
        t_span *span;
        int ispan = --span_alloc_1;
        span = span_pool + ispan;
        span->next = span_heads_1[c];
        span_heads_1[c] = ispan;
        // set span data
        span->ys = (unsigned char)rtri.ys;
        span->ye = (unsigned char)rtri.ye;
        span->fid = fc;
        //}
      }
    }
#else
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
#endif
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

  // ---- wait for previous frame to be done
  *LEDS = 0;
  wait_all_drawn();
  // ---- now can access texture memory

#ifdef DEBUG
  unsigned int tm_0 = time();
  num_clipped = 0;
#endif

  /// transform frustum in world space
  //*LEDS = 1;
  frustum_transform(&frustum_view, z_clip, inv_transform, unproject, &frustum_trsf);
  /// transform normals
  for (int n = 0; n < n_normals; ++n) {
    trsf_normals[n] = normals[n];
    transform(&trsf_normals[n].x,&trsf_normals[n].y,&trsf_normals[n].z,0);
  }
  /// transform texvecs
  for (int n = 0; n < n_texvecs; ++n) {
    trsf_texvecs[n] = texvecs[n];
    transform(&trsf_texvecs[n].vecS.x,
              &trsf_texvecs[n].vecS.y,
              &trsf_texvecs[n].vecS.z,0);
    transform(&trsf_texvecs[n].vecT.x,
              &trsf_texvecs[n].vecT.y,
              &trsf_texvecs[n].vecT.z,0);
  }
#ifdef DEBUG
  unsigned int tm_1 = time();
#endif
  /// locate current leaf
  //*LEDS = 2;
  unsigned short leaf = locate_leaf();
  printf("5 view %d,%d,%d (%d, %d) in leaf %d\n", view.x, view.y, view.z, v_angle_y, v_angle_x, leaf);
  /// get visibility list
#ifdef DEBUG
  unsigned int tm_2 = time();
#endif
  //*LEDS = 3;
  vfc_len = readLeafVisList(leaf);
  /// check frustum - aabb
#ifdef DEBUG
  unsigned int tm_3 = time();
#endif
  //*LEDS = 4;
  core1_done = 0;
  core1_todo = 2; // request core 1 assistance
  frustumTest(0,vfc_len>>1);
  while (core1_done != 1) {} // wait for core 1
  /// render visible leaves
#ifdef DEBUG
  unsigned int tm_4 = time();
#endif
  //*LEDS = 5;
  renderLeaves(vfc_len);

#ifdef DEBUG
  unsigned int tm_5 = time();
  tm_colprocess = 0;
  tm_srfspan = 0;
  tm_api = 0;
#endif

  //*LEDS = 6;
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

  //*LEDS = 7;

#ifdef DEBUG
  unsigned int tm_6 = time();
  printf("1 %d spans\n", span_alloc_0 + (MAX_NUM_SPANS - span_alloc_1));
  printf("2 %d rfaces (%d clipped)\n", rface_next_id_0 + (MAX_RASTER_FACES - rface_next_id_1),num_clipped);
  printf("3 trsf %d, loc %d, vis %d, vfc %d, render %d, spans %d (cols %d, srf %d, api %d)\n",
    tm_1 - tm_0, tm_2 - tm_1, tm_3 - tm_2, tm_4 - tm_3, tm_5 - tm_4, tm_6 - tm_5, tm_colprocess, tm_srfspan, tm_api);
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

  // prepare the frustum (slow)
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
  v_angle_x = 0;
#endif
  int v_start = view.z - 1000;
  int v_end   = view.z + 2100;
  int v_step  = 1;

  // view.x = 1920; view.y =  310;  view.z = -926; // e1m1
  // view.x = 3596; view.y = 3790;  view.z = 1719; // e1m4
  // view.x = 1918; view.y = 286;  view.z = 1295; // e1m6
  view.x = 5003; view.y = -1417;  view.z = 5956; v_angle_y=-10448; v_angle_x=209; // lmap

  while (1) {

    tm_frame = time();

    // draw screen
    render_frame();

    int tm = time();
    int elapsed = tm - tm_frame;
    int speed = (elapsed >> 17);

    prev_uart_byte = uart_byte();
    p3d front = { 0,0,256 };
    inv_transform(&front.x, &front.y, &front.z, 0);
    if (prev_uart_byte & 1) {
      view.x += speed * front.x >> 7; view.y += speed * front.y >> 7; view.z += speed * front.z >> 7;
    }
    if (prev_uart_byte & 2) {
      view.x -= speed * front.x >> 7; view.y -= speed * front.y >> 7; view.z -= speed * front.z >> 7;
    }
    if (prev_uart_byte & 4) {
      v_angle_y += speed<<1;
    }
    if (prev_uart_byte & 8) {
      v_angle_y -= speed<<1;
    }
    if (prev_uart_byte & 16) {
      view.y += speed;
    }
    if (prev_uart_byte & 32) {
      view.y -= speed;
    }
    if (prev_uart_byte & 64) {
      v_angle_x += speed;
    }
    if (prev_uart_byte & 128) {
      v_angle_x -= speed;
    }

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
