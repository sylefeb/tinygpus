// _____________________________________________________________________________
// |                                                                           |
// |  View frustum utilities                                                   |
// |                                                                           |
// |  include raster.c first                                                   |
// |___________________________________________________________________________|
// |                                                                           |
// | @sylefeb 2022-04-22  licence: GPL v3, see full text in repo               |
// | Contains square root code under MIT license (see links in code below)     |
// |___________________________________________________________________________|

// A plane
typedef struct {
	p3d   n;
	short d;
} plane;

// An axis aligned box
typedef struct {
	short min_x, min_y, min_z;
	short max_x, max_y, max_z;
} aabb;

// A frustum (no far plane)
typedef struct {
	plane planes[5];
} frustum;

// ____________________________________________________________________________
// Computes a plane from three points
static inline void plane_from_three_points(const p3d *p0, const p3d *p1, const p3d *p2, plane *pl)
{
	normal_from_three_points(p0, p1, p2, &pl->n);
	pl->d = - dot3(p0->x, p0->y, p0->z, pl->n.x, pl->n.y, pl->n.z) >> 8;
}

// ____________________________________________________________________________
// Builds the frustum planes
// not fast, meant to be computed once, planes are then transformed
void frustum_pre(
	frustum *f, 
	void (*f_unproject)(const p2d *, short, p3d*), 
	int z_clip)
{
	// unproject screen points
	p2d screen_pts[4] = {
		{0,            0},
		{SCREEN_WIDTH, 0},
		{SCREEN_WIDTH, SCREEN_HEIGHT},
		{0,            SCREEN_HEIGHT},
	};
	p3d near[4];
	for (int i = 0; i < 4; ++i) {
		f_unproject(screen_pts + i, z_clip, near + i);
	}
	p3d further[4];
	for (int i = 0; i < 4; ++i) {
		f_unproject(screen_pts + i, z_clip<<1, further + i);
	}
  /*       further
	      +----------+
	     /|         /|
	    / |       /  |
     /  +---- /  --+
	 3 + ----- + 2   /
		 | /     |   /
		 |/      | /
	 0 + ----- + 1
		    near
	
	*/
	// compute planes
	// -> front
	plane_from_three_points(near + 0, near    + 1, near    + 2, f->planes + 0);
	// -> left
	plane_from_three_points(near + 0, near    + 3, further + 0, f->planes + 1);
	// -> right
	plane_from_three_points(near + 1, further + 1, near    + 2, f->planes + 2);
	// -> top
	plane_from_three_points(near + 3, near    + 2, further + 3, f->planes + 3);
	// -> bottom
	plane_from_three_points(near + 0, further + 0, near    + 1, f->planes + 4);
}

// ____________________________________________________________________________
