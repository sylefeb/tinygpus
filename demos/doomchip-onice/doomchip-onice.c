// _____________________________________________________________________________
// |                                                                           |
// |  Doomchip 'on-ice' firmware. Kneep deep in the code.                      |
// |  ===================================================                      |
// |                                                                           |
// | This is a custom re-implementation of the Doom render loop.               |
// | Also supports terrains!                                                   |
// |                                                                           |
// | References:                                                               |
// | - "DooM unofficial specs" http://www.gamers.org/dhs/helpdocs/dmsp1666.html|
// | - "DooM black book" by Fabien Sanglard                                    |
// |                                                                           |
// | @sylefeb 2021-05-05  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

// include level header, automatically generated from WAD file
#include "../build/level.h"

#define SCREEN_WIDTH  doomchip_width
#define SCREEN_HEIGHT doomchip_height
#define TERRAIN_ID    255

// -----------------------------------------------------

// #define SPRITES

#include "api.c"

static inline int terrainh()
{
  return userdata()>>16;
}

static inline int dot3(int a, int b,int c, int x,int y,int z)
{
  return a*x + b*y + c*z;
}

// -----------------------------------------------------
// Geometry
// -----------------------------------------------------

static inline int side(
  int16 ray_dx,int16 ray_dy,
  int16 dx,int16 dy
) {
  return mul(dx,ray_dy) > mul(dy,ray_dx);
}

// -----------------------------------------------------

static inline int infront(
  int16 ray_dx,int16 ray_dy,
  int16 dx,int16 dy
) {
  return dot(dx,ray_dx, dy,ray_dy) > 0;
}

// -----------------------------------------------------

static inline int seg_frustum(int16 view_x,int16 view_y,
                        int16 ray_left_dx,int16 ray_left_dy,
                        int16 ray_front_dx,int16 ray_front_dy,
                        int16 ray_right_dx,int16 ray_right_dy,
                        int16 p0x,int16 p0y,
                        int16 p1x,int16 p1y
                      )
{
  int d0x = p0x - view_x;
  int d0y = p0y - view_y;
  int sl0 = side   (ray_left_dx, ray_left_dy,  d0x,d0y);
  int sr0 = side   (ray_right_dx,ray_right_dy, d0x,d0y);
  int fr0 = infront(ray_front_dx,ray_front_dy, d0x,d0y);

  int d1x = p1x - view_x;
  int d1y = p1y - view_y;
  int sl1 = side   (ray_left_dx, ray_left_dy,  d1x,d1y);
  int sr1 = side   (ray_right_dx,ray_right_dy, d1x,d1y);
  int fr1 = infront(ray_front_dx,ray_front_dy, d1x,d1y);

  return !( (!fr0 && !fr1) || (!sl0 && !sl1) || (sr0 && sr1) );
}

// -----------------------------------------------------

static inline int bbox_frustum(int16 view_x,int16 view_y,
                        int16 ray_left_dx,int16 ray_left_dy,
                        int16 ray_front_dx,int16 ray_front_dy,
                        int16 ray_right_dx,int16 ray_right_dy,
                        const struct bsp_bbox *bbox
                      )
{
  int d0x = bbox->x_lw - view_x;
  int d0y = bbox->y_lw - view_y;
  int d1x = bbox->x_hi - view_x;
  int d1y = bbox->y_lw - view_y;
  int d2x = bbox->x_hi - view_x;
  int d2y = bbox->y_hi - view_y;
  int d3x = bbox->x_lw - view_x;
  int d3y = bbox->y_hi - view_y;

  int sl0 = side   (ray_left_dx, ray_left_dy,  d0x,d0y);
  int sl1 = side   (ray_left_dx, ray_left_dy,  d1x,d1y);
  int sl2 = side   (ray_left_dx, ray_left_dy,  d2x,d2y);
  int sl3 = side   (ray_left_dx,ray_left_dy,   d3x,d3y);

  if (!sl0 && !sl1 && !sl2 && !sl3) { // all to the left
    return 0;
  }

  int sr0 = side   (ray_right_dx,ray_right_dy, d0x,d0y);
  int sr1 = side   (ray_right_dx,ray_right_dy, d1x,d1y);
  int sr2 = side   (ray_right_dx,ray_right_dy, d2x,d2y);
  int sr3 = side   (ray_right_dx,ray_right_dy, d3x,d3y);

  if ( sr0 &&  sr1 &&  sr2 &&  sr3) { // all to the right
    return 0;
  }

  int fr0 = infront(ray_front_dx,ray_front_dy, d0x,d0y);
  int fr1 = infront(ray_front_dx,ray_front_dy, d1x,d1y);
  int fr2 = infront(ray_front_dx,ray_front_dy, d2x,d2y);
  int fr3 = infront(ray_front_dx,ray_front_dy, d3x,d3y);

  if (!fr0 && !fr1 && !fr2 && !fr3) { // all behind
    return 0;
  }

  return 1;
}

// -----------------------------------------------------

static inline int bbox_ray(int16 view_x,int16 view_y,
                           int16 ray_dx,int16 ray_dy,
                           const struct bsp_bbox *bbox
                      )
{
  int d0x = bbox->x_lw - view_x;
  int d0y = bbox->y_lw - view_y;
  int s0 = side(ray_dx,ray_dy,  d0x,d0y);

  int d1x = bbox->x_hi - view_x;
  int d1y = bbox->y_lw - view_y;
  int s1 = side(ray_dx,ray_dy,  d1x,d1y);

  if (s0 ^ s1) return 1;

  int d2x = bbox->x_hi - view_x;
  int d2y = bbox->y_hi - view_y;
  int s2 = side(ray_dx,ray_dy,  d2x,d2y);

  if (s1 ^ s2) return 1;

  int d3x = bbox->x_lw - view_x;
  int d3y = bbox->y_hi - view_y;
  int s3 = side(ray_dx,ray_dy,  d3x,d3y);

  if (s2 ^ s3) return 1;
  if (s3 ^ s0) return 1;

  // NOTE: no need to test in front, implicit by bbox testing

  return 0;
}

// -----------------------------------------------------

static inline int bbox_touches(int16 px,int16 py,int16 radius,
                        const struct bsp_bbox *bbox
                      )
{
  if (px < bbox->x_lw - radius) return 0;
  if (px > bbox->x_hi + radius) return 0;
  if (py < bbox->y_lw - radius) return 0;
  if (py > bbox->y_hi + radius) return 0;
  return 1;
}

// -----------------------------------------------------
// Lighting
// -----------------------------------------------------

static inline int sectorLightLevel(int sec,int frame,int rand)
{
  switch (bspSectors[sec].special) {
    case 1: { // random off
      if (rand < 256) {
        return bspSectors[sec].lowlight; // off (lowlight)
      } else {
        return bspSectors[sec].light; // on (sector light)
      }
    }
    case 2: { // flash fast
      if ( ((frame/4)&3) == 0 ) {
        return bspSectors[sec].light;
      } else {
        return bspSectors[sec].lowlight;
      }
    }
    case 3: { // flash slow
      if ( ((frame/8)&3) == 0 ) {
        return bspSectors[sec].light;
      } else {
        return bspSectors[sec].lowlight;
      }
    }
    case 12: { // flash fast
      if ( ((frame/4)&3) == 0 ) {
        return bspSectors[sec].light;
      } else {
        return bspSectors[sec].lowlight;
      }
    }
    case 13: { // flash slow
      if ( ((frame/8)&3) == 0 ) {
        return bspSectors[sec].light;
      } else {
        return bspSectors[sec].lowlight;
      }
    }
    case 8: { // oscillates (to improve)
      if ( ((frame/8)&1) == 0) {
        return bspSectors[sec].lowlight;
      } else {
        return bspSectors[sec].light;
      }
    }
  }
  return bspSectors[sec].light;
}

// -----------------------------------------------------
// Fixed point
// -----------------------------------------------------

#define FPm         12
#define FPh         16
#define DEPTH_SHIFT 11

// -----------------------------------------------------
// Global game state
// -----------------------------------------------------

// config
const int terrain_z_offset = 64;
// frame
volatile int frame;
// rand gen
unsigned int rand;
// viewpoint
int view_x;
int view_y;
int view_z;
int view_a;
volatile int view_sector; // volatile when accessed by both cores
volatile int sector_f_T;
// frustum
int sinview;  int cosview;
int sinleft;  int cosleft;
int sinright; int cosright;

// helper function to compute screen height for columns
static inline int to_h(int h,int invD) {
  return (mul(h,invD)>>14) + (doomchip_height>>1);
  //                   ^^ depends on invd shift
}

// -----------------------------------------------------
// Visibility
// -----------------------------------------------------

#define N_VIS_SEC   128
#define N_VIS       512
#define MAX_SPRITES  64

int                    vis_ssec_next;
unsigned short         vis_ssecs      [N_VIS_SEC];
unsigned short         vis_p0         [N_VIS_SEC]; // NOTE could be made uchar using sub-frustrums
unsigned short         vis_p1         [N_VIS_SEC];
unsigned short         vis_begin      [N_VIS_SEC];
unsigned char          vis_len        [N_VIS_SEC];
unsigned char          vis_seclight   [N_VIS_SEC];

struct vis_record {
  unsigned short owner;
  int            invd;
  int            invd_inc;
  int            tu_invd;
  int            tu_invd_inc;
  unsigned short i0;
  unsigned short i1;
};

int                    vis_seg_next;
struct vis_record      vis            [N_VIS];

int                    num_level_vis_segs;

// unsigned int sector_vis[N_BSP_SECTORS/32+1];
unsigned int sector_vis[N_BSP_SECTORS];

int stack [64]; // stack for BSP traversal

// -----------------------------------------------------
// Visible sectors
// -----------------------------------------------------

static inline void sec_vis_reset()
{
  //for (int i = 0; i < (N_BSP_SECTORS/32+1); ++i) {
  //  sector_vis[i] = 0;
  //}
  for (int i = 0; i < N_BSP_SECTORS; ++i) {
    //if (sector_vis[i]) {
    //  printf("%d,",i);
    //}
    sector_vis[i] = 0;
  }
  //printf("\n");
}

static inline void sec_vis_add(int sec)
{
  // sector_vis[sec>>5] = sector_vis[sec>>5] | (1 << (sec&31));
  sector_vis[sec] = 1;
}

static inline int sec_vis_test(int sec)
{
  // return sector_vis[sec>>5] & (1 << (sec&31));
  return sector_vis[sec];
}

// -----------------------------------------------------
// Visible segments
// -----------------------------------------------------

// Add a segment to the list of visible segments to be drawn
static inline int add_segment(
  const struct p2d v0,const struct p2d v1,int slen,int max_vis_segs)
{
  // prepare segment for display
  if (seg_frustum(view_x,view_y,
                  cosleft,sinleft, cosview,sinview, cosright,sinright,
                  v0.x,v0.y, v1.x,v1.y)) {
    // add to vis
    if (vis_seg_next < max_vis_segs) {
      // project
      int d0x = v0.x - view_x;
      int d0y = v0.y - view_y;
      int d1x = v1.x - view_x;
      int d1y = v1.y - view_y;
      // -> rotate in view
      int rx0       = - dot(d0y, cosview, - d0x, sinview) >> FPm;
      int rx1       = - dot(d1y, cosview, - d1x, sinview) >> FPm;
      int ry0       =   dot(d0x, cosview,   d0y, sinview) >> FPm;
      int ry1       =   dot(d1x, cosview,   d1y, sinview) >> FPm;
      const int near = 16;
      int in_front0 = ry0 >= near;
      int in_front1 = ry1 >= near;
      // at least one in front
      if (in_front0 || in_front1) {
        // texturing
        int tu0     = 0;
        int tu1     = slen;
        // -> clip if necessary
        if (!in_front0 || !in_front1) {
          // swap to have the one not in front as 0
          // => we always clip 0
          int clip_dir = 1;
          if (!in_front1) {
            int tmp  = rx0; rx0 = rx1; rx1 = tmp;
            tmp      = ry0; ry0 = ry1; ry1 = tmp;
            tmp      = tu0; tu0 = tu1; tu1 = tmp;
            clip_dir = -1;
          }
          // compute ratio
          int ratio;
          ratio    = div( (near - ry0) << FPh , ry1 - ry0 );
          // update x0
          rx0      = rx0 + (mul( ratio , rx1 - rx0 ) >> FPh);
          ry0      = near;
          // update texture u coord
          tu0     += mul(clip_dir, mul( ratio , slen ) >> FPh);
        }
        // -> project
        int p0,p1;
        p0     = (div(mul(rx0,3*doomchip_width/4),ry0)) + doomchip_width/2;
        p1     = (div(mul(rx1,3*doomchip_width/4),ry1)) + doomchip_width/2;
        // -> swap to store in order
        if (p0 > p1) {
          int tmp = p0;  p0   = p1;  p1   = tmp;
          tmp     = ry0; ry0  = ry1; ry1  = tmp;
          tmp     = tu0; tu0  = tu1; tu1  = tmp;
        }
        // -> compute interpolation ratio
        // -> setup invd interpolation
        int invd0       = div( (1<<22) , ry0 );
        int invd1       = div( (1<<22) , ry1 );
        int invd        = invd0;
        int invd_inc    = div( (invd1 - invd0), (p1-p0) );
        int tu_invd0    = mul( tu0, invd0 ) >> 6;
        int tu_invd1    = mul( tu1, invd1 ) >> 6;
        int tu_invd     = tu_invd0;
        int tu_invd_inc = div( (tu_invd1 - tu_invd0), (p1 - p0));
        // columns to draw, clamp to screen
        int i0 = p0;
        int i1 = p1;
        if (i1 < 0) {
          return -1;
        } else if (i0 >= doomchip_width) {
          return -1;
        } else {
          if (i0 < 0) {
            // invd        += mul(-i0, invd_inc);    // NOTE: bad, accum error
            invd     = invd0    - div( i0 * (invd1 - invd0), (p1-p0) );
            // tu_invd     += mul(-i0, tu_invd_inc); // NOTE: bad, accum error
            tu_invd  = tu_invd0 - div( i0 * (tu_invd1 - tu_invd0), (p1 - p0));
            // reset i0
            i0      = 0;
          }
        }
        // insert the ssec vis record
        vis[vis_seg_next] = (struct vis_record){
          .owner       = -1,
          .invd        = invd,
          .invd_inc    = invd_inc,
          .tu_invd     = tu_invd,
          .tu_invd_inc = tu_invd_inc,
          .i0          = i0,
          .i1          = i1
        };

        // add to vis list
        return vis_seg_next ++;

      }

    } else {
#ifdef SIMULATION
			printf("too many vis segments (%d / %d)\n",vis_seg_next, max_vis_segs);
#endif
      // too many vis segments
      return -1;
    }
  }
  return -1;
}

// -----------------------------------------------------

// Traverses the BSP and collects the potentially visible sub-sectors
// Transforms and projects all potentially visible segments
static inline void bsp_pvs()
{
  // traverse BSP (sort sectors)
  int stack_ptr    = 0;
  stack [stack_ptr] = root;
  stack_ptr         = stack_ptr + 1;
  vis_ssec_next     = 0;

  while (stack_ptr != 0) {
    // pop
    stack_ptr = stack_ptr - 1;
    int n    = stack[stack_ptr];

    if ((n & 32768) == 0) {

      // tree node
      int dx  = view_x - bspNodes[n].x;
      int dy  = view_y - bspNodes[n].y;
      int csl = mul(dx , bspNodes[n].dy);
      int csr = mul(dy , bspNodes[n].dx);
      if (csr > csl) {
        stack[stack_ptr] = bspNodes[n].rchild;
        stack_ptr        = stack_ptr + 1;
        if (bbox_frustum(view_x,view_y,
                        cosleft,sinleft, cosview,sinview, cosright,sinright,
                        &bspNodes[n].lbb)) {
          stack[stack_ptr] = bspNodes[n].lchild;
          stack_ptr        = stack_ptr + 1;
        }
      } else {
        stack[stack_ptr]  = bspNodes[n].lchild;
        stack_ptr         = stack_ptr + 1;
        if (bbox_frustum(view_x,view_y,
                        cosleft,sinleft, cosview,sinview, cosright,sinright,
                        &bspNodes[n].rbb)) {
          stack[stack_ptr] = bspNodes[n].rchild;
          stack_ptr        = stack_ptr + 1;
        }
      }

    } else {

      // subsector reached
      int ssec     = n & 32767;
      int n_before = vis_seg_next;
      int ssec_i0  = doomchip_width;
      int ssec_i1  = 0;

			// for each segment
      for (int s = 0; s < bspSSectors[ssec].num_segs ; s++) {
        int seg       = bspSSectors[ssec].start_seg + s;
        struct p2d v0 = bspSegs[seg].v0;
        struct p2d v1 = bspSegs[seg].v1;
        int    slen   = bspSegs[seg].seglen;
        // try to add segment
        int vid = add_segment(v0,v1,slen, N_VIS-MAX_SPRITES);
        if (vid != -1) {
          // set owner
          vis[vid].owner = seg;
          // track min-max projected columns for the ssec (sub-sector)
          int i0 = vis[vid].i0;
          int i1 = vis[vid].i1;
          if (i0 < ssec_i0) ssec_i0 = i0;
          if (i1 > ssec_i1) ssec_i1 = i1;
        }
      }

      int n_added = vis_seg_next - n_before;

      if (n_added) {
        if (vis_ssec_next < N_VIS_SEC) {
          vis_ssecs   [vis_ssec_next] = ssec;
          vis_p0      [vis_ssec_next] = ssec_i0;
          vis_p1      [vis_ssec_next] = ssec_i1;
          vis_len     [vis_ssec_next] = n_added;
          int sec                     = bspSSectors[ssec].parentsec;
          vis_seclight[vis_ssec_next] = sectorLightLevel(sec,frame,rand);
          ++ vis_ssec_next;
        } else {
          // too many sub-sectors
          break;
        }
      }
    }
  }
  num_level_vis_segs = vis_seg_next;
}

// -----------------------------------------------------
// Finds the sector containing a point
// -----------------------------------------------------

static inline int find_sector(int px,int py)
{
  // descend BSP
  int n = root;
  while (1) {
    if ((n & 32768) == 0) {
      // tree node
      int dx  = px - bspNodes[n].x;
      int dy  = py - bspNodes[n].y;
      int csl = mul(dx , bspNodes[n].dy);
      int csr = mul(dy , bspNodes[n].dx);
      if (csr > csl) {
        n = bspNodes[n].lchild;
      } else {
        n = bspNodes[n].rchild;
      }
    } else {
      // subsector reached
      int ssec = n & 32767;
      return bspSSectors[ssec].parentsec;
    }
  }
  return -1;
}

// -----------------------------------------------------
// Sprites
// -----------------------------------------------------

struct sprite {
  unsigned char  thing;
  int            dist;
  unsigned short sec;
  unsigned char  light;
	unsigned char  w;
  unsigned char  h;
  unsigned char  frame;
  unsigned char  flags;
};

int           num_sprites;
struct sprite sprites[MAX_SPRITES];

// Selects a sprite frame based on angle
void spriteSelect(int angle,int ani_frame,int *sprt_frame,int *sptr_mirror)
{
  // angle in 0 - 4095
  if (angle < 256) {
    *sprt_frame  = mul(ani_frame,5) + 2;
    *sptr_mirror = 1;
  } else { if (angle < 768) {
    *sprt_frame  = mul(ani_frame,5) + 3;
    *sptr_mirror = 1;
  } else { if (angle < 1280) {
    *sprt_frame  = mul(ani_frame,5) + 4;
    *sptr_mirror = 0;
  } else { if (angle < 1792) {
    *sprt_frame  = mul(ani_frame,5) + 3;
    *sptr_mirror = 0;
  } else { if (angle < 2304) {
    *sprt_frame  = mul(ani_frame,5) + 2;
    *sptr_mirror = 0;
  } else { if (angle < 2816) {
    *sprt_frame  = mul(ani_frame,5) + 1;
    *sptr_mirror = 0;
  } else { if (angle < 3328) {
    *sprt_frame  = mul(ani_frame,5) + 0;
    *sptr_mirror = 0;
  } else { if (angle < 3840) {
    *sprt_frame  = mul(ani_frame,5) + 1;
    *sptr_mirror = 1;
  } else {
    *sprt_frame  = mul(ani_frame,5) + 2;
    *sptr_mirror = 1;
  } } } } } } } }
}

// goes through things and add those potentially visible as sprites
static inline void add_sprites()
{
  num_sprites = 0;
  for (int t = 0; t < n_things ; ++t) {
    int sprt_x    = things[t].x;
    int sprt_y    = things[t].y;
    int sec       = find_sector(sprt_x,sprt_y);
    if (!sec_vis_test(sec)) {
      continue;
    }
    int dimid     = things[t].first_frame - first_sprite_index;
    int sprt_w    = sprtdims[(dimid<<1)+0];
    int sprt_h    = sprtdims[(dimid<<1)+1];
    int sprt_r    = sprt_w;
    int sprt_vx   = mul(sprt_r,-sinview) >> (FPm+1);
    int sprt_vy   = mul(sprt_r, cosview) >> (FPm+1);
    struct p2d p0 = {.x = sprt_x-sprt_vx, .y = sprt_y-sprt_vy};
    struct p2d p1 = {.x = sprt_x+sprt_vx, .y = sprt_y+sprt_vy};
    // try to add segment
    int vid = add_segment(p0,p1,sprt_r,N_VIS);
    if (vid != -1) {
      // precompute distance
      int y = div( 1<<25, vis[vid].invd );
      // verify not too close
      if (y < 64) {
        -- vis_seg_next; // remove
        continue;
      }
      // set parent
      sprites[num_sprites].thing = t;
      // set distance
      sprites[num_sprites].dist  = y;
      // set width,height
			sprites[num_sprites].w     = sprt_w;
      sprites[num_sprites].h     = sprt_h;
      // set sector
      sprites[num_sprites].sec   = sec;
      sprites[num_sprites].light = sectorLightLevel(sprites[num_sprites].sec,frame,rand);
      // reset flags
      sprites[num_sprites].flags = 0;
      // set frame
      if (things[t].flags&1) {
        int sprt_frame,sprt_mirror;
        int sel_angle = (1024 + view_a - (things[t].a<<5)) & 4095;
        spriteSelect(sel_angle, 0, &sprt_frame,&sprt_mirror );
        sprites[num_sprites].frame = things[t].first_frame + sprt_frame;
        sprites[num_sprites].flags = sprt_mirror;
      } else {
        sprites[num_sprites].frame = things[t].first_frame;
      }
      // set owner
      vis[vid].owner = num_sprites;
      // next
      ++ num_sprites;
      if (num_sprites == MAX_SPRITES) {
#ifdef SIMULATION
        printf("WARNING: out of sprites\n");
#endif
        break; // can't take any more sprites
      }
    }
  }
#ifdef SIMULATION
  //printf("%d sprites in frame\n",num_sprites);
#endif
}

// draw sprites in a screen column
static inline void draw_sprites_column(int c)
{
  for (int v = num_level_vis_segs; v < vis_seg_next; v++) {
    if (c >= vis[v].i0 && c <= vis[v].i1) {
      // perspective correct interpolation
      int sprt       = vis[v].owner;
      // -> inv distance
      int c_delta    = c - vis[v].i0;
      int invd       = vis[v].invd; // constant across sprite
      int tu_invd    = vis[v].tu_invd + mul(c_delta,vis[v].tu_invd_inc);
      // -> distance (view y coordinate)
      int y          = sprites[sprt].dist;   // distance
      int tc_u       = mul(tu_invd,y) >> 19; // tex u coord
			// -> mirrored?
			if (sprites[sprt].flags) {
				tc_u = sprites[sprt].w - tc_u;
			}
      // -> sprite height
      int sprt_sec   = sprites[sprt].sec;
      int floor_h    = bspSectors[sprt_sec].f_h - view_z;
      int sprt_z_btm = to_h(floor_h,invd);
      int sprt_z_top = to_h(floor_h + sprites[sprt].h,invd);
      // -> clamp
      int tc_v       = 0;
      if (sprt_z_btm < 0) {
        tc_v       = mul((0 - sprt_z_btm),y) >> DEPTH_SHIFT;
        sprt_z_btm = 0;
      }
      if (sprt_z_top >= doomchip_height) {
        sprt_z_top = doomchip_height-1;
      }
      int tex_id = sprites[sprt].frame;
      col_process();
      col_send(
        COLDRAW_WALL(y,tc_v,tc_u),
        COLDRAW_COL(tex_id,sprt_z_btm,sprt_z_top, sprites[sprt].light) | WALL
      );
    }
  }
}

// -----------------------------------------------------
// Determine collisions
// -----------------------------------------------------

static inline struct p2d collision(int px,int py,int radius)
{
  // descend tree, using bbox to decide which ssec to test
  int colx   = px;
  int coly   = py;
  int stack_ptr    = 0;
  stack [stack_ptr] = root;
  stack_ptr         = stack_ptr + 1;
  while (stack_ptr != 0) {
    // pop
    stack_ptr = stack_ptr - 1;
    int n    = stack[stack_ptr];
    if ((n & 32768) == 0) {
      // tree node
      int dx  = px - bspNodes[n].x;
      int dy  = py - bspNodes[n].y;
      int csl = mul(dx , bspNodes[n].dy);
      int csr = mul(dy , bspNodes[n].dx);
      if (csr > csl) {
        stack[stack_ptr] = bspNodes[n].rchild;
        stack_ptr        = stack_ptr + 1;
        if (bbox_touches(px,py,radius,
                        &bspNodes[n].lbb)) {
          stack[stack_ptr] = bspNodes[n].lchild;
          stack_ptr        = stack_ptr + 1;
        }
      } else {
        stack[stack_ptr]  = bspNodes[n].lchild;
        stack_ptr         = stack_ptr + 1;
        if (bbox_touches(px,py,radius,
                        &bspNodes[n].rbb)) {
          stack[stack_ptr] = bspNodes[n].rchild;
          stack_ptr        = stack_ptr + 1;
        }
      }
    } else {
      // subsector reached
      int ssec     = n & 32767;
      // go through all segments
      for (int s = 0; s < bspSSectors[ssec].num_segs ; s++) {
        int seg       = bspSSectors[ssec].start_seg + s;
        struct p2d v0 = bspSegs[seg].v0;
        struct p2d v1 = bspSegs[seg].v1;
        // solid wall?   TODO: correct test, also take heights into account
        if (bspSegs[seg].mid != 0) {
          int d0x = colx - v0.x;
          int d0y = coly - v0.y;
          int ldx = v1.x - v0.x;
          int ldy = v1.y - v0.y;
          int ndx = - ldy; // ndx,ndy is orthogonal to v1 - v0
          int ndy =   ldx;
          // dot products
          int l_h = dot(ldx,d0x, ldy,d0y);
          int d_h = dot(ndx,d0x, ndy,d0y);
          if (d_h < 0) { // TODO: two-sided walls
            int segl   = bspSegs[seg].seglen;
            int seglsq = mul(segl,segl);
            int cold   = mul(segl,radius);
            if ( -d_h < cold
              && l_h > -radius && l_h < seglsq + radius) {
              int uncol = - (cold + d_h)<<FPm; // recall d_h < 0
              uncol = div(uncol,segl);
              colx  = colx + (div(mul(uncol,ndx),segl) >> FPm);
              coly  = coly + (div(mul(uncol,ndy),segl) >> FPm);
            }
          }
        }
      }
    }
  }
  return (struct p2d){.x=colx,.y=coly};
}

// -----------------------------------------------------
// Draw all screen columns
// -----------------------------------------------------

// terrain ray-caster call
static inline void terrain(
  int view_x, int view_y, int view_z,      /// TODO: clean up unused params
  int start_x,int start_y,int start_dist,
  int end_x,  int end_y,  int end_dist,
  int btm,    int top,    int col)
{
  int pick = (col == 160 && start_dist == 0) ? PICK : 0;
  col_send(
    COLDRAW_TERRAIN(start_dist,end_dist,pick),
    COLDRAW_COL    (terrain_texture_id, btm, top, 15) | TERRAIN
  );
}

// -----------------------------------------------------

// draws all screen columns
static inline void draw_columns()
{
  for (int c = 0 ; c != doomchip_width ; ++c) {
#ifdef SIMULATION
    //printf("Column %d -------------------------\n",c);
#endif

    int prev_x    = view_x;
    int prev_y    = view_y;
    int prev_dist = 0;
    int sin_view  = sin_m[(col_to_alpha[c] + 1024) & 4095];
    int inv_sw    = div(1<<FPm,sin_view);             // TODO: precomp!!!

    int colangle  = view_a + col_to_alpha[c];
    int ray_dy    = sin_m[ colangle         & 4095];
    int ray_dx    = sin_m[(colangle + 1024) & 4095];

    // pre-mult for perspective distance correction
    ray_dx        = mul(ray_dx,inv_sw);
    ray_dy        = mul(ray_dy,inv_sw);
    col_send(
      PARAMETER_RAY_CS(ray_dx,ray_dy),
      PARAMETER
    );
    // apply view rotation to flats (planes)
    int rz = 4096;
    int cx = col_to_x[c];
    int du = dot3( cx,0,rz,  cosview,0,sinview ) >> 14;
    int dv = dot3( cx,0,rz, -sinview,0,cosview ) >> 14;
    col_send(
      PARAMETER_PLANE_A(256,0,0), // ny,uy,vy
      PARAMETER_PLANE_DTA(du,dv) | PARAMETER
    );

    // init top/btm
    int top       = doomchip_height - 1;
    int btm       = 0;

    // for each vis sub-sector
    int v = 0;
    int v_end;
    for (int ssc = 0; ssc < vis_ssec_next && top > btm ; ++ssc, v = v_end) {

      v_end = v + vis_len[ssc];

      if (c >= vis_p0[ssc]) { if (c <= vis_p1[ssc]) {

      // for each vis segment
      for ( ; v < v_end ; ++v ) {

        if (c >= vis[v].i0 && c <= vis[v].i1) {

          int sec        = bspSSectors[vis_ssecs[ssc]].parentsec;
          int seg        = vis[v].owner;
#ifdef SPRITES
          // tag sector as visible
          sec_vis_add(sec);
#endif
          // perspective correct interpolation
          // -> inv distance
          int c_delta    = c - vis[v].i0;
          int invd       = vis[v].invd    + mul(c_delta,vis[v].invd_inc);
          int tu_invd    = vis[v].tu_invd + mul(c_delta,vis[v].tu_invd_inc);
          // -> distance (view y coordinate)
          int y          = div( 1<<25     , invd );       // distance
          int tc_u       = mul(tu_invd,y) >> 19; // tex u coord
          int y_low      = y>>3;
          // hit point
          int hit_dist   = mul(y_low,inv_sw);
          int hit_x      = view_x + (mul(ray_dx,y_low) >> FPm);
          int hit_y      = view_y + (mul(ray_dy,y_low) >> FPm);
          // sector light
          int seclight = vis_seclight[ssc];
          // sector floor/ceiling height
          int sec_f_h = bspSectors[sec].f_h - view_z;
          int sec_c_h = bspSectors[sec].c_h - view_z;
          int f_h     = to_h(sec_f_h,invd);
          int c_h     = to_h(sec_c_h,invd);
          // process pending column commands
          col_process();
          // adjust tex coord
          int tex_v_f = 0;
          if (btm > f_h) {
            tex_v_f = mul((btm - f_h),y) >> DEPTH_SHIFT;
            f_h     = btm;
          } else if (top < f_h) {
            tex_v_f = mul((f_h - top),y) >> DEPTH_SHIFT;
            f_h     = top;
          }
          if (btm > c_h) {
            c_h     = btm;
          } else if (top < c_h) {
            c_h     = top;
          }

          // floor or terrain?
          if (bspSectors[sec].f_T) {
            // floor with flat texturing
            int is_terrain = bspSectors[sec].f_T == TERRAIN_ID;
            if (!is_terrain) {
              col_send(
                COLDRAW_PLANE_B(-sec_f_h,btm-(doomchip_height>>1)),
                COLDRAW_COL(bspSectors[sec].f_T,btm,f_h, seclight) | PLANE
              );
            } else {
              // trace terrain
              terrain(view_x,view_y,view_z,
                      prev_x,prev_y,prev_dist,
                      hit_x,hit_y,hit_dist,
                      btm,top, c);
            }
          }
          btm       = f_h;
          // ceiling with flat texturing
          col_send(
            COLDRAW_PLANE_B( sec_c_h,c_h-(doomchip_height>>1)),
            COLDRAW_COL(bspSectors[sec].c_T,c_h,top, seclight) | PLANE
          );
          top       = c_h;

          int other = bspSegs[seg].other_sec;

          // lower wall
          if (bspSegs[seg].lwr) {
            int is_terrain = bspSegs[seg].lwr == TERRAIN_ID;
            int sec_f_o    = bspSectors[other].f_h - view_z;
            int f_o        = to_h(sec_f_o,invd);
            if (btm > f_o) {
              f_o = btm;
            } else if (top < f_o) {
              f_o = top;
            }
            if (!is_terrain) {
              col_send(
                COLDRAW_WALL(y,tex_v_f,tc_u),
                COLDRAW_COL(bspSegs[seg].lwr,btm,f_o, seclight) | WALL
              );
            } else if (hit_dist > prev_dist) {
              // trace terrain
              terrain(view_x,view_y,view_z,
                      prev_x,prev_y,prev_dist,
                      hit_x,hit_y,((2<<12)-1),
                      btm,f_o, c);
              // background filler
              col_send(
                COLDRAW_WALL(Y_MAX,0,0),
                COLDRAW_COL(0, btm,f_o, 15) | WALL
              );
            }
            btm            = f_o;
          }

          // process pending column commands
          col_process();

          // upper wall
          if (bspSegs[seg].upr) {
            int is_terrain = bspSegs[seg].upr == TERRAIN_ID;
            int sec_c_o    = bspSectors[other].c_h - view_z;
            int c_o        = to_h(sec_c_o,invd);
            int tex_v      = 0;
            if (btm > c_o) {
              tex_v   = mul((btm - c_o),y) >> DEPTH_SHIFT;
              c_o     = btm;
            } else if (top < c_o) {
              tex_v   = mul((c_o - top),y) >> DEPTH_SHIFT;
              c_o     = top;
            }
            if (!is_terrain) {
              col_send(
                COLDRAW_WALL(y,tex_v,tc_u),
                COLDRAW_COL(bspSegs[seg].upr, c_o,top, seclight) | WALL
              );
            } else if (hit_dist > prev_dist) {
              // trace terrain
              terrain(view_x,view_y,view_z,
                      prev_x,prev_y,prev_dist,
                      hit_x,hit_y,((2<<12)-1),
                      c_o,top, c);
              // background filler
              col_send(
                COLDRAW_WALL(Y_MAX,0,0),
                COLDRAW_COL(0, c_o,top, 15) | WALL
              );
            }
            top            = c_o;
          }

          // middle wall
          if (bspSegs[seg].mid) {
            int is_terrain = bspSegs[seg].mid == TERRAIN_ID;
            if (!is_terrain) {
              col_send(
                COLDRAW_WALL(y,tex_v_f,tc_u),
                COLDRAW_COL(bspSegs[seg].mid, f_h,c_h, seclight) | WALL
              );
              // close column?
              if ((bspSegs[seg].flags&1) == 0) {
                //              ^^^^^ transparent if flags&1
                top = btm; // opaque, close column
                break;
              }
            } else if (hit_dist > prev_dist) {
              // trace terrain
              terrain(view_x,view_y,view_z,
                      prev_x,prev_y,prev_dist,
                      hit_x,hit_y,((2<<12)-1),
                      f_h,c_h, c);
              // background filler
              col_send(
                COLDRAW_WALL(Y_MAX,0,0),
                COLDRAW_COL(0, f_h,c_h, 15) | WALL
              );
              // terrain is the last thing we render
              top = btm;
              break;
            }
          }

          // terrain hits
          if (hit_dist > prev_dist) {
            // ^^^^^^^^^^^^^^^^^^^ segments are not sorted within a sector
            // so we may process a segment located in front after we already
            // advanced to the background
            // track hit points
            prev_x    = hit_x;
            prev_y    = hit_y;
            prev_dist = hit_dist;
          }

        } // intersection

      } // vis segments

      }} // active sector

    } // vis sub-sectors

#ifdef SPRITES
    // render sprite columns
    draw_sprites_column(c);
#endif

    // take a deep breath
    col_process();

    if (btm < top) {
      // still open, add a filler with sky texture (id == 0)
      col_send(
              COLDRAW_WALL(Y_MAX,0,0),
              COLDRAW_COL(0, btm, top, 15) | WALL
            );
    }

    // send end of column
    col_send(0, COLDRAW_EOC);

  } // c
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
  *LEDS = 16;

  // --------------------------
  // intialize view and frame
  // --------------------------
  view_x   = player_start_x;
  view_y   = player_start_y;
  view_z   = 30; //40;
  view_a   = player_start_a + 128;
  frame    = 0;
  rand     = 3137;
#ifdef SPRITES
  sec_vis_reset();
#endif

  // --------------------------
  // init oled
  // --------------------------
  oled_init();
  oled_fullscreen();

  // --------------------------
  // render loop
  // --------------------------
  while (1) {

		{ // get frustum
		int angle = view_a;
		sinview   = sin_m[ angle         & 4095];
		cosview   = sin_m[(angle + 1024) & 4095];
		angle     = view_a + col_to_alpha[0];
		sinleft   = sin_m[ angle         & 4095];
		cosleft   = sin_m[(angle + 1024) & 4095];
		angle     = view_a + col_to_alpha[doomchip_width-1];
		sinright  = sin_m[ angle         & 4095];
		cosright  = sin_m[(angle + 1024) & 4095];
		}

		{ // adjust view altitude
		int player_sec = find_sector(view_x,view_y);
		if (bspSectors[player_sec].f_T != TERRAIN_ID) {
			view_z    = bspSectors[player_sec].f_h + 30;
		} else {
			view_z    = terrainh() - terrain_z_offset + 30;
		}
		}

    // reset vis segments
    vis_seg_next = 0;

    // potential visible set from BSP
    bsp_pvs();

#ifdef SPRITES
    // add sprites (using visibility from previous frame)
    add_sprites();
    // reset sprite visibnility
    sec_vis_reset();
#endif

    // before drawing cols wait for previous frame
    while ((userdata()&4) == 0) { /*wait*/ }

    // setup view
    *VIEW_POS_Z  =  view_z + terrain_z_offset;
    //                       ^^^^ view_z used for terrain only, this allows a
    //                            global offset on terrain altitude
    col_send(
      PARAMETER_UV_OFFSET( view_y, view_x ),
      PARAMETER
    );

    // draw screen columns
    draw_columns();

    // update rand seed
    rand   = rand * 31421 + 6927;

    // user input
    if (btn_left()) {
      view_a -= 54;
    }
    if (btn_right()) {
      view_a += 54;
    }
    if (btn_fwrd()) {
      view_x += cosview>>8;
      view_y += sinview>>8;
    }

//#ifdef SIMULATION
    // random walk around
    if ((frame&63) < 40) {
      if (((frame>>6)&1) == 0) {
        view_a += (rand&127);
      } else {
        view_a -= (rand&127);
      }
    }
    view_x += cosview>>8;
    view_y += sinview>>8;
//#endif

    // uncollide
    struct p2d pos = collision(view_x,view_y,40);
    view_x  = pos.x;
    view_y  = pos.y;

    // next frame
    ++ frame;

  } // while (1)

}

// -----------------------------------------------------

void main()
{
  if (core_id()) {
		main_1();
	} else {
		main_0();
	}
}

// -----------------------------------------------------
