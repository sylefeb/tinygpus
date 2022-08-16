// _____________________________________________________________________________
// |                                                                           |
// |  A rasterizer API for the DMC-! GPU.                                      |
// |                                                                           |
// |                                                                           |
// | Implements rasterization of convex faces (triangles, and more).           |
// |                                                                           |
// |  This is meant to rasterize textured *columns* (y-spans) of constant x    |
// |  (while most soft-rasters typically do x-spans of constant y).            |
// |                                                                           |
// | Texturing is perspective correct using planar texture coordinates.        |
// |                                                                           |
// | Two important concepts are used here:                                     |
// | (1)- the rasterized polygons are *convex*                                 |
// | (2)- the rasterized polygons are *oriented*                               |
// |                                                                           |
// | (1): Triangles always are convex, many other polygons are and this avoids |
// |      having to 'cut' them down into triangles.                            |
// |                                                                           |
// |       /----\          /-----\                                             |
// |      /      \        /       \                                            |
// |      \      /        \    /--/                                            |
// |       \----/          \--/                                                |
// |       convex        non-convex (concave)                                  |
// |                                                                           |
// | (2): Following the vertex numbering, the rotation around the polygon goes |
// |      either clock-wise (CW) or counter-clock-wise (CCW)                   |
// |                                                                           |
// |       0-->--1          0--<--3                                            |
// |      /       \        /       \                                           |
// |     ^         v      v         ^                                          |
// |      \       /        \       /                                           |
// |       3--<--2          1-->--2                                            |
// |         CW               CCW                                              |
// |                                                                           |
// |  This allows to define front facing (CW) and back facing (CCW) polygons.  |
// |  Polygons are created such that seen from the outside they are (e.g.) CW. |
// |  This way, a polygon seen from the back appears CCW. Such Backfacing      |
// |  polygons will not be drawn, which is typical in 3D rendering of closed   |
// |  shapes and environments ; it reduces per-frame polygon count by ~half    |
// |  (think of a sphere: we do not need to render the hemisphere oriented     |
// |  towards the back).                                                       |
// |  Another important implication is that any drawn polygon will have this   |
// |  property that from the leftmost vertex, the top edges are found by       |
// |  going to the next (+1) vertices (according to the orientation) while the |
// |  bottom edges are found by going to the previous (-1) vertices:           |
// |                                                                           |
// |         1 .-------. 2                                                     |
// |          /    ^    \                                                      |
// |         /  +1 top   \                                                     |
// |        /             \                                                    |
// |     0 .               . 3                                                 |
// |     ^  \             /                                                    |
// |   left  \ -1 bottom /                                                     |
// |   most   \    v    /                                                      |
// |        5  .------ . 4                                                     |
// |                                                                           |
// |  This polygon has N=6 vertices ; going -1 from 0 brings us to point 5,    |
// |  because we are reasoning modulo N around the polygon loop.               |
// |                                                                           |
// |___________________________________________________________________________|
// |                                                                           |
// | @sylefeb 2022-04-22  licence: GPL v3, see full text in repo               |
// | Contains square root code under MIT license (see links in code below)     |
// |___________________________________________________________________________|

// A 3D point, int coordinates
typedef struct {
	int x, y, z;
} p3d;

// ____________________________________________________________________________
//
// A rasterization edge (a side of a rasterized convex polygon)
//
//      x   x_end
//      :  /
//      : /
//      :/
//      o  <-- current rasterization point on edge
//     /^
//    / | dy/dx (by how much dy increases when dx increments by one?)
//   /  |
//  /---^
//
typedef struct {
	int x;     // current x
  int x_end; // end x
	int y;     // current y (16.16 bits fixed point)
  int dydx;  // slope     (16.16 bits fixed point)
} redge;

// ____________________________________________________________________________
//
// The rendered span will be between two redge, one top and one bottom:
//
//       x   x_end
//       :  /
//       : /
//       :/
//       o  <-- current rasterization point on top edge
//      /|
//     / |
//    /  |
//   .   |  <<< vertical span to render for column x
//    \  |
//     \ |
//      \|
//       o  <-- current rasterization point on bottom edge
//       :\
//       : \
//       :  x_end

// Table of 1/dx, avoids div in many cases
unsigned short inv_dx[SCREEN_WIDTH];

// Pre-computes tables for rasterization
static inline void raster_pre()
{
  inv_dx[0] = 0;
  inv_dx[1] = 65535;
  for (int x=2;x<SCREEN_WIDTH;++x) {
    inv_dx[x] = 65536/x;
  }
}

// ____________________________________________________________________________
// Initializes a redge, given the two endpoints x0,y0 and x1,y1
static inline void redge_init(redge *l, int x0, int y0, int x1, int y1)
{
	if (x1 < x0) {       // make sure x0 is first, so that x1-x0 is positive
    int tmp = x0; x0 = x1; x1 = tmp;
    tmp = y0; y0 = y1; y1 = tmp;
	}
	l->x_end = x1;       // start point of redge
  l->x     = x0;       // current x position
	l->y     = y0 << 16; // current y pos, 16.16
  int dx   = x1 - x0;  // x difference
  if (dx < SCREEN_WIDTH) { // < SCREEN_WIDTH,  avoid div and use table
	  l->dydx = (y1 - y0) * (int)(inv_dx[dx]);
  } else {             // >= SCREEN_WIDTH, use div
    l->dydx = ((y1 - y0) << 16) / dx;
  }
  // clip line if x is negative
  if (l->x < 0) {
    l->y += - l->x * l->dydx;
    l->x  = 0;
  }
}

// ____________________________________________________________________________
// Are we done rasterizing this line?
static inline int redge_done(const redge *l)
{
	return (l->x >= l->x_end);
}

// ____________________________________________________________________________
// Step the line rasterization
static inline void redge_step(redge *l)
{
	++l->x;          // increment x coordinate by 1
	l->y += l->dydx; // increment y coordinate by dydx (16.16 fixed point)
}

// ____________________________________________________________________________
// Convex face being rasterized
typedef struct {
	int   x;            // current x coordinate
  int   last_x;       // last x coordinate (done after that)
	short ys, ye;       // current y-start and y-end vertical span coordinates
	redge edge_top;     // edge currently at the top    (intersected along x)
  redge edge_btm;     // edge currently at the bottom (intersected along x)
	unsigned char vbtm; // first vertex of line at bottom (lower y)
	unsigned char vtop; // first vertex of line at top (higher y)
} rconvex;

// ____________________________________________________________________________
// computes the next index in cw order along the convex polygon
static inline int next(int i,int N) {
  ++i;
  if (i == N) { i = 0; }
  return i;
}

// ____________________________________________________________________________
// Computes the next index in ccw order along the convex polygon
static inline int prev(int i,int N) {
  --i;
  if (i == -1) { i = N-1; }
  return i;
}

// ____________________________________________________________________________
static inline void rconvex_init(
  rconvex *t, int nindices, const int *indices, const p3d *pts
) {
  // find leftmost points
  int min_x = pts[indices[0]].x;
  int max_x = pts[indices[0]].x;
  int left_most = 0;
  for ( int i = 1; i < nindices; ++i) {
    int x = pts[indices[i]].x;
    if ( x < min_x ) {
      left_most = i;
      min_x     = x;
    } else if ( x > max_x ) {
      max_x     = x;
    }
  }
  t->x      = min_x;
  if (t->x < 0) { t->x = 0; } // redge_init properly deals with this too
  t->last_x = max_x;
  if (t->last_x >= SCREEN_WIDTH) { t->last_x = SCREEN_WIDTH-1; }
  // Both lines start on the same left most vertex because the contour is
  // expected cw and the polygon front facing, there is no ambiguity that
  // the 'top' line is v->top,v->top+1 and 'btm' is v->btm,v->btm-1
  t->vtop   = left_most;
  t->vbtm   = left_most;
  int left_most_prev = prev(left_most,nindices);
  int left_most_next = next(left_most,nindices);
  // prepare both lines
  redge_init(&t->edge_top,
             pts[indices[left_most]].x,      pts[indices[left_most]].y,
             pts[indices[left_most_next]].x, pts[indices[left_most_next]].y);
  redge_init(&t->edge_btm,
             pts[indices[left_most_prev]].x, pts[indices[left_most_prev]].y,
             pts[indices[left_most]].x,      pts[indices[left_most]].y);
}

// ____________________________________________________________________________
static inline int rconvex_step(
  rconvex *t, int nindices, const int *indices, const p3d *pts)
{
  t->ys = t->edge_top.y >> 16;
  t->ye = t->edge_btm.y >> 16;
  // clamp y (assumes triangle is not out-of-screen)
  if (t->ye > SCREEN_HEIGHT - 1) t->ye = SCREEN_HEIGHT - 1;
  else if (t->ye < 0) t->ye = 0;
  if (t->ys > SCREEN_HEIGHT - 1) t->ys = SCREEN_HEIGHT - 1;
  else if (t->ys < 0) t->ys = 0;
  // increment
  ++t->x;
  if (t->x == t->last_x) {
    return 0;
  }
  redge_step(&t->edge_top);
  redge_step(&t->edge_btm);
  int t_done = 0;
  int b_done = 0;
  if (redge_done(&t->edge_top)) {
    t->vtop = next(t->vtop, nindices);
    t_done = 1;
  }
  if (redge_done(&t->edge_btm)) {
    t->vbtm = prev(t->vbtm, nindices);
    b_done = 1;
  }
  if (t_done) {
    int n = next(t->vtop, nindices);
    redge_init(&t->edge_top, pts[indices[t->vtop]].x, pts[indices[t->vtop]].y,
      pts[indices[n]].x, pts[indices[n]].y);
  }
  if (b_done) {
    int p = prev(t->vbtm, nindices);
    redge_init(&t->edge_btm, pts[indices[p]].x, pts[indices[p]].y,
      pts[indices[t->vbtm]].x, pts[indices[t->vbtm]].y);
  }
  return 1;
}

// ____________________________________________________________________________
// Definition of surfaces, which hold texture coordinate information
// for a texturing plane

typedef struct {
  int nx,ny,nz;
  int ux,uy,uz;
  int vx,vy,vz;
} surface;

typedef struct {
  int nx,ny,nz;
  int ux,uy,uz;
  int vx,vy,vz;
} trsf_surface;

typedef struct {
  int u_offs,v_offs;
  int ded;
} rconvex_texturing;

typedef int          int32_t;
typedef unsigned int uint32_t;

// ____________________________________________________________________________
// Square root code from https://github.com/chmike/fpsqrt/blob/master/fpsqrt.c
// MIT License, see https://github.com/chmike/fpsqrt/blob/master/LICENSE
//
// sqrt_i32 computes the squrare root of a 32bit integer and returns
// a 32bit integer value. It requires that v is positive.
int32_t sqrt_i32(int32_t v) {
    uint32_t b = 1<<30, q = 0, r = v;
    while (b > r)
        b >>= 2;
    while( b > 0 ) {
        uint32_t t = q + b;
        q >>= 1;
        if( r >= t ) {
            r -= t;
            q += b;
        }
        b >>= 2;
    }
    return q;
}

// ____________________________________________________________________________
// A good old dot product
static inline int dot3(int a, int b,int c, int x,int y,int z)
{
  return a*x + b*y + c*z;
}

// ____________________________________________________________________________
// A good old cross product
static inline void cross(
	int x0,int y0,int z0,
	int x1,int y1,int z1,
	int *x, int *y, int *z)
{
  *x = (y0*z1 - y1*z0) >> 8;
  *y = (z0*x1 - z1*x0) >> 8;
  *z = (x0*y1 - x1*y0) >> 8;
}

// ____________________________________________________________________________
// Normalizes a vector (makes it a unit vector, length == 1)
static inline void normalize(int *x, int *y, int *z)
{
  int lensq = dot3(*x,*y,*z, *x,*y,*z);
  int len   = sqrt_i32(lensq);
  *x        = ((*x) << 8) / len;
  *y        = ((*y) << 8) / len;
  *z        = ((*z) << 8) / len;
}

// ____________________________________________________________________________
// Prepares a surface from a triangle
static inline void surface_pre(surface *s,int p0,int p1,int p2,const p3d *pts)
{
  // compute triangle normal
  int d10x = pts[p1].x - pts[p0].x;
  int d10y = pts[p1].y - pts[p0].y;
  int d10z = pts[p1].z - pts[p0].z;
  int d20x = pts[p2].x - pts[p0].x;
  int d20y = pts[p2].y - pts[p0].y;
  int d20z = pts[p2].z - pts[p0].z;
  cross(d10x,d10y,d10z,    d20x,d20y,d20z,    &s->nx,&s->ny,&s->nz);
  normalize(&s->nx,&s->ny,&s->nz);
  // compute u,v from n
  cross(s->nx,s->ny,s->nz, 256,0,0,           &s->ux,&s->uy,&s->uz);
  normalize(&s->ux,&s->uy,&s->uz);
  cross(s->nx,s->ny,s->nz, s->ux,s->uy,s->uz, &s->vx,&s->vy,&s->vz);
  normalize(&s->vx,&s->vy,&s->vz);
}

// ____________________________________________________________________________
// Transform callback
typedef void (*f_transform)(int *x,int *y,int *z,int w);

// ____________________________________________________________________________
// Transform a surface with the current transform and view distance
static inline void surface_transform(const surface *s,trsf_surface *ts,
                                     f_transform trsf)
{
  ts->nx = s->nx; ts->ny = s->ny; ts->nz = s->nz;
  ts->ux = s->ux; ts->uy = s->uy; ts->uz = s->uz;
  ts->vx = s->vx; ts->vy = s->vy; ts->vz = s->vz;
  // transform plane vectors
  trsf(&ts->nx,&ts->ny,&ts->nz,0);
  trsf(&ts->ux,&ts->uy,&ts->uz,0);
  trsf(&ts->vx,&ts->vy,&ts->vz,0);
}

// ____________________________________________________________________________
// Prepares texturing info for a rconvex

static inline void rconvex_texturing_pre(
                                     const trsf_surface *ts, f_transform trsf,
                                     const p3d *p0, rconvex_texturing *rtex)
{
  // transform p0 (reference point for the transformed surface)
  p3d trp0     = *p0;
  trsf(&trp0.x,&trp0.y,&trp0.z,1);
  // uv translation: translate so that p0 uv coordinates remain (0,0)
  rtex->u_offs   = dot3( trp0.x,trp0.y,trp0.z, ts->ux,ts->uy,ts->uz );
  rtex->v_offs   = dot3( trp0.x,trp0.y,trp0.z, ts->vx,ts->vy,ts->vz );
  // plane distance
  rtex->ded      = dot3( trp0.x,trp0.y,trp0.z, ts->nx,ts->ny,ts->nz ) >> 8;
  // NOTE: ded < 0 ==> backface surface
  if (rtex->ded > 0) {
    rtex->u_offs = - rtex->u_offs;
    rtex->v_offs = - rtex->v_offs;
  }
}

// ____________________________________________________________________________
// Binds the surface for rendering (sets UV parameters)
static inline void rconvex_texturing_bind(const rconvex_texturing *rtex)
{
#ifndef EMUL
  col_send(
    PARAMETER_UV_OFFSET(rtex->v_offs),
    PARAMETER_UV_OFFSET_EX(rtex->u_offs) | PARAMETER
  );
#endif
}

// ____________________________________________________________________________
// Setup surface parameters for the span (return the dr parameter)
static inline int surface_setup_span(trsf_surface *s,int rx,int ry,int rz)
{
  int dr = dot3( rx,ry,rz, s->nx,s->ny,s->nz )>>8;
  int du = dot3( rx,ry,rz, s->ux,s->uy,s->uz )>>8;
  int dv = dot3( rx,ry,rz, s->vx,s->vy,s->vz )>>8;
#ifndef EMUL
  col_send(
    PARAMETER_PLANE_A(s->ny,s->uy,s->vy),
    PARAMETER_PLANE_A_EX(du,dv) | PARAMETER
  );
#endif
  return dr;
}


// ____________________________________________________________________________
// Polygon clipping ; if the polygon has z coordinates bloew the near z plane
// in view space, it must be clipped so that only the front part remains.
void clip_polygon(f_transform trsf, int nindices, const int *indices,
                  const p3d *pts, int z_clip, p3d *_clipped,int *_n_clipped)
  //                                                ^^^^^^       ^^^^^^
  //                         clipped up to nindices+2            |
  //                                                assumes intialized to zero
{
  p3d prev_p    = *(pts + indices[nindices-1]); // last
  trsf(&prev_p.x, &prev_p.y, &prev_p.z, 1);
  *_n_clipped = 0;
  int prev_back = (prev_p.z < z_clip);
  for (int i = 0; i < nindices; ++i) {
    p3d p = *(pts + *(indices++));
    trsf(&p.x, &p.y, &p.z, 1);
    int back = (p.z < z_clip);
    if (back ^ prev_back) { // crossing
      p3d prev_to_p;
      // delta vector
      prev_to_p.x = p.x - prev_p.x;
      prev_to_p.y = p.y - prev_p.y;
      prev_to_p.z = p.z - prev_p.z;
      // interpolation ratio
      int ratio = ((z_clip - prev_p.z) << 8) / (p.z - prev_p.z);
      // new point
      _clipped->x = prev_p.x + ((prev_to_p.x * ratio) >> 8);
      _clipped->y = prev_p.y + ((prev_to_p.y * ratio) >> 8);
      _clipped->z = prev_p.z + ((prev_to_p.z * ratio) >> 8);
      ++_clipped;
      ++(*_n_clipped);
    }
    if (!back) {
      _clipped->x = p.x;
      _clipped->y = p.y;
      _clipped->z = p.z;
      ++_clipped;
      ++(*_n_clipped);
    }
    prev_back = back;
    prev_p    = p;
  }
}

// ____________________________________________________________________________
