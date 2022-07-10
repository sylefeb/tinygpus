// _____________________________________________________________________________
// |                                                                           |
// |  Simple terrain demo                                                      |
// |  ===================                                                      |
// |                                                                           |
// | Draws a Comanche style terrain using the DMC-1 GPU                        |
// |                                                                           |
// | @sylefeb 2021-05-05  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#include "col_to_alpha_table.h"
#include "sine_table.h"
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
// Fixed point
// -----------------------------------------------------

#define FPm    12

// -----------------------------------------------------
// Global game state
// -----------------------------------------------------

// config
const int terrain_texture_id = 663;
const int terrain_z_offset = 32;
// frame
volatile int frame;
// rand gen
unsigned int rand;
// viewpoint
int view_x;
int view_y;
int view_z;
int view_a;

// -----------------------------------------------------

// terrain ray-caster call
static inline void terrain(
  int start_dist, int end_dist,
  int btm,    int top,    int col)
{
#ifdef SIMULATION
  //printf("Terrain draw call\n");
#endif
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
  for (int c = 0 ; c != SCREEN_WIDTH ; ++c) {

    int prev_x    = view_x;
    int prev_y    = view_y;
    int prev_dist = 0;
    int sin_view  = sine_table[(col_to_alpha_table[c] + 1024) & 4095];
    int inv_sw    = div(1<<FPm,sin_view);             // TODO: precomp!!!

    int colangle  = view_a + col_to_alpha_table[c];
    int ray_dy    = sine_table[ colangle         & 4095];
    int ray_dx    = sine_table[(colangle + 1024) & 4095];

    // pre-mult for perspective distance correction
    ray_dx        = mul(ray_dx,inv_sw);
    ray_dy        = mul(ray_dy,inv_sw);
    col_send(
      PARAMETER_RAY_CS(ray_dx,ray_dy),
      PARAMETER
    );

    // trace terrain
    terrain(0,2047, 0,239, c);

    // background filler
    col_send(
      COLDRAW_WALL(Y_MAX,0,0),
      COLDRAW_COL(0, 0,239, 15) | WALL
    );

    // take a deep breath
    col_process();

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
  *LEDS = 0;

  // --------------------------
  // intialize view and frame
  // --------------------------
  view_x   = 16384;
  view_y   = 0;
  view_z   = 0;
  view_a   = 0;
  frame    = 0;
  rand     = 3137;

  // --------------------------
  // init oled
  // --------------------------
  oled_init();
  oled_fullscreen();

  // --------------------------
  // render loop
  // --------------------------
  while (1) {

#if defined(SIMULATION)
    // some good old debug printf
    printf("[%d] view %d,%d %d  z=%d\n",frame,
            view_x,view_y,view_a,view_z);
#endif

		int sinview   = sine_table[ view_a         & 4095];
		int cosview   = sine_table[(view_a + 1024) & 4095];

		// adjust view altitude
		view_z    = terrainh() - terrain_z_offset + 30;

    // before drawing cols wait for previous frame
    wait_all_drawn();

    // setup view
    col_send(
      PARAMETER_VIEW_Z( view_z + terrain_z_offset ),
      //                          ^^^^
      //                          global offset on terrain altitude
      PARAMETER
    );
    col_send(
      PARAMETER_UV_OFFSET(view_x,view_y),
      PARAMETER
    );

    // draw screen columns
    draw_columns();

    // update rand seed
    rand   = rand * 31421 + 6927;

#if 0
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
#else
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
#endif

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
