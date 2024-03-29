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
	p3d n;
	int d;
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

int side(const plane *pl, int x, int y, int z)
{
	return (dot3(x,y,z, pl->n.x, pl->n.y, pl->n.z) >> 8) + pl->d;
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

	// TEST
	/*
	p2d pctr = { SCREEN_WIDTH / 2,SCREEN_HEIGHT / 2 };
	p3d ptest;
	f_unproject(&pctr, z_clip << 2, &ptest);
	for (int i = 0; i < 5; ++i) {
		int s = side(f->planes + i, ptest.x, ptest.y, ptest.z);
		printf("side: %d\n",s);
	}
	*/
}

// ____________________________________________________________________________
// Transform the frustum
void frustum_transform(const frustum *src, int z_clip,
	void (*inv_transform)(short *x, short *y, short *z, short w),
	void (*f_unproject)(const p2d *, short, p3d*),
	frustum *trsf)
{
	for (int i = 0; i < 5; ++i) {
		// transform normal
		trsf->planes[i].n = src->planes[i].n;
		inv_transform(&trsf->planes[i].n.x,&trsf->planes[i].n.y,&trsf->planes[i].n.z, 0);
		// recompute d
		/// TODO: find something faster
		// point on plane
		p3d o;
		o.x = - ((int)src->planes[i].n.x * src->planes[i].d) >> 8;
		o.y = - ((int)src->planes[i].n.y * src->planes[i].d) >> 8;
		o.z = - ((int)src->planes[i].n.z * src->planes[i].d) >> 8;
		// transform the point from view space to world space
		inv_transform(&o.x, &o.y, &o.z, 1);
		// compute distance
		trsf->planes[i].d = - dot3(o.x, o.y, o.z,
			                         trsf->planes[i].n.x, trsf->planes[i].n.y, trsf->planes[i].n.z) >> 8;
	}

	/*
	// TEST
	p2d pctr = { SCREEN_WIDTH / 2,SCREEN_HEIGHT / 2 };
	p3d ptest;
	f_unproject(&pctr, z_clip << 2, &ptest);
	inv_transform(&ptest.x, &ptest.y, &ptest.z, 1);
	for (int i = 0; i < 5; ++i) {
		int s = side(trsf->planes + i, ptest.x, ptest.y, ptest.z);
		printf("side: %d\n", s);
	}
	*/
}

// ____________________________________________________________________________
// AABB - frustum overlap test
// based on https://old.cescg.org/CESCG-2002/DSykoraJJelinek/

// order in aabb is 0:min_x,1:min_y,2:min_z, 3:max_x,4:max_y,5:max_z
typedef struct { int px, py, pz, nx, ny, nz; } t_np_vertices;
t_np_vertices np_vertices_lkup[8] = {
	{ 3,4,5, 0,1,2 }, // 0 0 0 all positive
	{ 3,4,2, 0,1,5 },
	{ 3,1,5, 0,4,2 },
	{ 3,1,2, 0,4,5 },
	{ 0,4,5, 3,1,2 },
	{ 0,4,2, 3,1,5 },
	{ 0,1,5, 3,4,2 },
	{ 0,1,2, 3,4,5 },
};
// returns the config for the above lookup table, from a plane normal
static inline int np_config(const p3d *n)
{
	int cfg = 0;
	if (n->x < 0) cfg += 4;
	if (n->y < 0) cfg += 2;
	if (n->z < 0) cfg += 1;
	return cfg;
}
// cases that can result from the overlap test
#define OUTSIDE    0
#define INSIDE     1
#define INTERSECT -1
// compute the overlap case
int frustum_aabb_overlap(const aabb *bx, const frustum *f)
{
	const short *bv = (const short *)bx;
	int result      = INSIDE;
  for (int i = 0; i < 5; i++) {
		const plane         *pl  = f->planes + i;
		int icfg                 = np_config(&pl->n);
		const t_np_vertices *cfg = np_vertices_lkup + icfg;
		int p_side = side(pl, bv[cfg->px], bv[cfg->py], bv[cfg->pz]);
		if (p_side < 0) {
			return OUTSIDE;
		}
		int n_side = side(pl, bv[cfg->nx], bv[cfg->ny], bv[cfg->nz]);
		if (n_side < 0) {
			result = INTERSECT;
		}
	}
	return result;
}
