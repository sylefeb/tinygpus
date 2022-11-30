// _____________________________________________________________________________
// |                                                                           |
// | Back to the roots! let's directly load the BSP like it's 1996             |
// |                                                                           |
// | This tool extracts and repacks what's needed for the DMC-1 q5k demo       |
// | It's messy, with no concern whatsoever regarding performance.             |
// |                                                                           |
// | See also [1] "The Most Unofficial Quake Technical Specification"          |
// |   https://www.gamers.org/dEngine/quake/spec/quake-spec34/qkmenu.htm       |
// |   Brian Martin, Raphael Quinet, Nicholas Dwarkanath,                      |
// |   John Wakelin, David Etherton, and others                                |
// | Struct defs are taken from there                                          |
// |                                                                           |
// | Another great source of info: https://fabiensanglard.net/quakeSource/     |
// | https://github.com/fabiensanglard/Quake--QBSP-and-VIS                     |
// |                                                                           |
// |___________________________________________________________________________|
// |                                                                           |
// | @sylefeb 2022-04-30  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

#include <LibSL/LibSL.h>

#include "tga.h"
#include <algorithm>

using namespace std;
using namespace LibSL;

#pragma pack(1)

// --------------------------------------------------------------

#define MAP "e1m1.bsp"
v3f view_pos = v3f(480.0f, 48.0f, 88.0f); // leaf 1116 (e1m1)

// --------------------------------------------------------------
const float scale = 4.0f; // global scale, adjusted for DMC-1
// --------------------------------------------------------------

// From [1]
typedef struct                 // A Directory entry
{
  long  offset;                // Offset to entry, in bytes, from start of file
  long  size;                  // Size of entry in file, in bytes
} dentry_t;
//
typedef struct                 // The BSP file header
{
  long  version;               // Model version, must be 0x17 (23).
  dentry_t entities;           // List of Entities.
  dentry_t planes;             // Map Planes.
                               // numplanes = size/sizeof(plane_t)
  dentry_t miptex;             // Wall Textures.
  dentry_t vertices;           // Map Vertices.
                               // numvertices = size/sizeof(vertex_t)
  dentry_t visilist;           // Leaves Visibility lists.
  dentry_t nodes;              // BSP Nodes.
                               // numnodes = size/sizeof(node_t)
  dentry_t texinfo;            // Texture Info for faces.
                               // numtexinfo = size/sizeof(texinfo_t)
  dentry_t faces;              // Faces of each surface.
                               // numfaces = size/sizeof(face_t)
  dentry_t lightmaps;          // Wall Light Maps.
  dentry_t clipnodes;          // clip nodes, for Models.
                               // numclips = size/sizeof(clipnode_t)
  dentry_t leaves;             // BSP Leaves.
                               // numlaves = size/sizeof(leaf_t)
  dentry_t lface;              // List of Faces.
  dentry_t edges;              // Edges of faces.
                               // numedges = Size/sizeof(edge_t)
  dentry_t ledges;             // List of Edges.
  dentry_t models;             // List of Models.
                               // nummodels = Size/sizeof(model_t)
} dheader_t;
//
typedef float scalar_t;        // Scalar value,
//
typedef struct                 // Vector or Position
{
  scalar_t x;                  // horizontal
  scalar_t y;                  // horizontal
  scalar_t z;                  // vertical
} vec3_t;
//
typedef struct                 // Bounding Box, Float values
{
  vec3_t   min;                // minimum values of X,Y,Z
  vec3_t   max;                // maximum values of X,Y,Z
} boundbox_t;
//
typedef struct                 // Bounding Box, Short values
{
  short   min_x,min_y,min_z;   // minimum values of X,Y,Z
  short   max_x,max_y,max_z;   // maximum values of X,Y,Z
} bboxshort_t;
//
typedef struct
{
  boundbox_t bound;            // The bounding box of the Model
  vec3_t origin;               // origin of model, usually (0,0,0)
  long node_id0;               // index of first BSP node
  long node_id1;               // index of the first Clip node
  long node_id2;               // index of the second Clip node
  long node_id3;               // usually zero
  long numleafs;               // number of BSP leaves
  long face_id;                // index of Faces
  long face_num;               // number of Faces
} model_t;
//
typedef vec3_t vertex_t;
//
typedef struct
{ u_short vertex0;             // index of the start vertex
                               //  must be in [0,numvertices[
  u_short vertex1;             // index of the end vertex
                               //  must be in [0,numvertices[
} edge_t;
//
typedef struct
{ vec3_t   vectorS;            // S vector, horizontal in texture space)
  scalar_t distS;              // horizontal offset in texture space
  vec3_t   vectorT;            // T vector, vertical in texture space
  scalar_t distT;              // vertical offset in texture space
  u_long   texture_id;         // Index of Mip Texture
                               //           must be in [0,numtex[
  u_long   animated;           // 0 for ordinary textures, 1 for water
} surface_t;
//
typedef struct
{ u_short plane_id;            // The plane in which the face lies
                               //           must be in [0,numplanes[
  u_short side;                // 0 if in front of the plane, 1 if behind the plane
  long ledge_id;               // first edge in the List of edges
                               //           must be in [0,numledges[
  u_short ledge_num;           // number of edges in the List of edges
  u_short texinfo_id;          // index of the Texture info the face is part of
                               //           must be in [0,numtexinfos[
  u_char typelight;            // type of lighting, for the face
  u_char baselight;            // from 0xFF (dark) to 0 (bright)
  u_char light[2];             // two additional light models
  long lightmap;               // Pointer inside the general light map, or -1
                               // this define the start of the face light map
} face_t;
//
typedef struct
{ long    plane_id;            // The plane that splits the node
                               //           must be in [0,numplanes[
  u_short front;               // If bit15==0, index of Front child node
                               // If bit15==1, ~front = index of child leaf
  u_short back;                // If bit15==0, id of Back child node
                               // If bit15==1, ~back =  id of child leaf
  bboxshort_t box;             // Bounding box of node and all childs
  u_short face_id;             // Index of first Polygons in the node
  u_short face_num;            // Number of faces in the node
} node_t;
//
typedef struct
{ long type;                   // Special type of leaf
  long vislist;                // Beginning of visibility lists
                               //     must be -1 or in [0,numvislist[
  bboxshort_t bound;           // Bounding box of the leaf
  u_short lface_id;            // First item of the list of faces
                               //     must be in [0,numlfaces[
  u_short lface_num;           // Number of faces in the leaf
  u_char sndwater;             // level of the four ambient sounds:
  u_char sndsky;               //   0    is no sound
  u_char sndslime;             //   0xFF is maximum volume
  u_char sndlava;              //
} dleaf_t;
//
typedef struct
{
  vec3_t   normal;             // Vector orthogonal to plane (Nx,Ny,Nz)
                               // with Nx2+Ny2+Nz2 = 1
  scalar_t dist;               // Offset to plane, along the normal vector.
                               // Distance from (0,0,0) to the plane
  long     type;               // Type of plane, depending on normal vector.
} plane_t;
//
typedef struct                 // Mip texture list header
{
  long  numtex;                // Number of textures in Mip Texture list
  long *offset;                // Offset to each of the individual texture
} mipheader_t;                 //  from the beginning of mipheader_t
//
typedef struct                 // Mip Texture
{
  char   name[16];             // Name of the texture.
  u_long width;                // width of picture, must be a multiple of 8
  u_long height;               // height of picture, must be a multiple of 8
  u_long offset1;              // offset to u_char Pix[width   * height]
  u_long offset2;              // offset to u_char Pix[width/2 * height/2]
  u_long offset4;              // offset to u_char Pix[width/4 * height/4]
  u_long offset8;              // offset to u_char Pix[width/8 * height/8]
} miptex_t;
//

// --------------------------------------------------------------
// Globals ( yeah, well ... just a quick tool ;) )

FILE     *f = NULL; // file
dheader_t h;        // header

// --------------------------------------------------------------

template <typename T> long offset()            { sl_assert(false); return -1; }
template <>           long offset<model_t>()   { return h.models.offset; }
template <>           long offset<plane_t>()   { return h.planes.offset; }
template <>           long offset<node_t>()    { return h.nodes.offset; }
template <>           long offset<face_t>()    { return h.faces.offset; }
template <>           long offset<edge_t>()    { return h.edges.offset; }
template <>           long offset<vertex_t>()  { return h.vertices.offset; }
template <>           long offset<dleaf_t>()   { return h.leaves.offset; }
template <>           long offset<surface_t>() { return h.texinfo.offset; }

template <typename T>
void read(int i,T *m)
{
  fseek(f, offset<T>() + sizeof(T) * i, SEEK_SET);
  fread(m, sizeof(T), 1, f);
}

short readLEdge(int pos)
{
  fseek(f, h.ledges.offset + 2 * sizeof(short) * pos, SEEK_SET);
  //                        ^^^ why?
  short s;
  fread(&s, sizeof(short), 1, f);
  return s;
}

u_short readLFace(int pos)
{
  fseek(f, h.lface.offset + sizeof(u_short) * pos, SEEK_SET);
  u_short s;
  fread(&s, sizeof(u_short), 1, f);
  return s;
}

v3f to_v3f(vertex_t v) { return v3f(v.x, v.y, v.z); }

// --------------------------------------------------------------

// find which BSP leaf contains a position
u_short locateLeaf(v3f pos, u_short root)
{
  u_short nid = root;
  while (1) {
    // reached a leaf?
    if (nid & 0x8000) {
      // return leaf index
      return ~nid;
    }
    // get node and plane
    node_t nd;
    read(nid, &nd);
    plane_t pl;
    read(nd.plane_id, &pl);
    // compute side
    float side = dot(pos,to_v3f(pl.normal)) - pl.dist;
    if (side < 0.0f) {
      nid = nd.back;
    } else {
      nid = nd.front;
    }
  }
}

// --------------------------------------------------------------

// get all leaf faces as triangles
void getLeafTriangles(int leaf, std::vector<Tuple<v3f, 3> >& _tris,int& _nfaces)
{
  // read leaf
  dleaf_t lf;
  read(leaf, &lf);
  // read faces
  for (int i = 0; i < lf.lface_num; ++i) {
    ++_nfaces;
    u_short fid = readLFace(lf.lface_id + i);
    face_t fc;
    read(fid, &fc);
    v3f first;
    for (int e = 0; e < fc.ledge_num; ++e) {
      short eid = readLEdge(fc.ledge_id + e);
      edge_t eg;
      read(eid < 0 ? -eid : eid, &eg);
      vertex_t p0;
      vertex_t p1;
      read(eg.vertex0, &p0);
      read(eg.vertex1, &p1);
      if (eid < 0) {
        std::swap(p0, p1);
      }
      if (e == 0) {
        first = to_v3f(p0);
      } else {
        Tuple<v3f, 3> tri;
        tri[0] = first;
        tri[1] = to_v3f(p0);
        tri[2] = to_v3f(p1);
        _tris.push_back(tri);
      }
    }
  }
}

// --------------------------------------------------------------

// get all leaf faces as convex polygons
void getLeafFaces(int leaf, std::vector< vector<v3f> >& _faces, std::vector<int>& _face_ids)
{
  // read leaf
  dleaf_t lf;
  read(leaf, &lf);
  // get texture header
  fseek(f, h.miptex.offset, SEEK_SET);
  long numtex;
  fread(&numtex, sizeof(long), 1, f);
  vector<long> offsets;
  offsets.resize(numtex);
  fread(&offsets[0], sizeof(long), numtex, f);
  // read faces
  for (int i = 0; i < lf.lface_num; ++i) {
    face_t fc;
    u_short fid = readLFace(lf.lface_id + i);
    read(fid, &fc);
    // check texture to eliminate trigger faces
    surface_t snfo;
    read(fc.texinfo_id, &snfo);
    miptex_t tnfo;
    fseek(f, h.miptex.offset + offsets[snfo.texture_id], SEEK_SET);
    fread(&tnfo, sizeof(tnfo), 1, f);
    if (!strcmp(tnfo.name, "trigger")) {
      continue;
    }
    // get face
    _faces.push_back(vector<v3f>());
    _face_ids.push_back(fid);
    v3f first;
    for (int e = 0; e < fc.ledge_num; ++e) {
      short eid = readLEdge(fc.ledge_id + e);
      edge_t eg;
      read(eid < 0 ? -eid : eid, &eg);
      vertex_t p0;
      vertex_t p1;
      read(eg.vertex0, &p0);
      read(eg.vertex1, &p1);
      if (eid < 0) {
        std::swap(p0, p1);
      }
      _faces.back().push_back(to_v3f(p0));
    }
    // reverse
    reverse(_faces.back().begin(), _faces.back().end());
  }
}

// --------------------------------------------------------------

uchar *vlist = nullptr; // vislist global header

// get vis list for a leaf
void getLeafVislist(int leaf, std::vector<int>& _vis)
{
  // read leaf
  dleaf_t lf;
  read(leaf, &lf);
  // parse vislist, https://www.gamers.org/dEngine/quake/spec/quake-spec34/qkspec_4.htm#BL4
  int v = lf.vislist;
  int numleaves = h.leaves.size / sizeof(dleaf_t);
  if (vlist == nullptr) {
    vlist = new uchar[h.visilist.size];
    fseek(f, h.visilist.offset, SEEK_SET);
    fread(vlist, 1, h.visilist.size, f);
  }
  for (int L = 1; L < numleaves; v++)
  {
    if (vlist[v] == 0)          // value 0, leaves invisible
    {
      L += 8 * vlist[v + 1];    // skip some leaves
      v++;
    } else                      // tag 8 leaves, if needed
    {                           // examine bits right to left
      for (uchar bit = 1; bit != 0; bit = bit * 2, L++) {
        if (vlist[v] & bit) {
          if (L < numleaves) {
            _vis.push_back(L);
          }
        }
      }
    }
  }
}

// --------------------------------------------------------------

// checks whether a vertex is already known in uniquep (very slow!)
int search_vertex(v3f p, const vector<v3f>& uniquep)
{
  for (int i = 0; i < uniquep.size(); ++i) {
    if (length(p - uniquep[i]) < 0.5f) {
      return i;
    }
  }
  return -1;
}

// compares two planar texture defs
bool cmp_uv_vec(v4f a, v4f b)
{
  return (dot(v3f(a), v3f(b)) > 0.999f && fabs(a[3]-b[3]) < 0.1f);
}

// checks whether a planar texture def is already known in uniques (very slow!)
int search_surface(pair<v4f,v4f> srf, const vector<pair<v4f, v4f> >& uniques)
{
  for (int i = 0; i < uniques.size(); ++i) {
    if ( cmp_uv_vec(srf.first, uniques[i].first)
      && cmp_uv_vec(srf.second,uniques[i].second)) {
      return i;
    }
  }
  return -1;
}

// checks whether a normal is already known in uniquen (very slow!)
int search_normal(v3f n, const vector<v3f>& uniquen)
{
  for (int i = 0; i < uniquen.size(); ++i) {
    if (dot(n, uniquen[i]) > 0.999f) {
      return i;
    }
  }
  return -1;
}

// swaps y/z since in the demo the view is along z, not y
void coord_swap(v3i& p) { std::swap(p[1], p[2]); }
void coord_swap(v3s& p) { std::swap(p[1], p[2]); }

// computes a face normal
v3f face_normal(const vector<v3f>& f)
{
  v3f n;
  int i = -1;
  do {
    ++i;
    v3f a = f[i + 1] - f[i + 0];
    v3f b = f[i + 2] - f[i + 0];
    n = normalize_safe(cross(a, b));
  } while (n == v3f(0.0f) && i + 2 < f.size());
  if (n == v3f(0.0f)) {
    fprintf(stderr, "cannot compute face normal\n");
    exit(-1);
  }
  return n;
}

// load a palette from a RIFF file
void load_palette(const char *fpal,unsigned char *pal)
{
  FILE *f = fopen(fpal, "rb");
  if (f == NULL) { return; }
  char sig[4];
  fread(sig, 1, 4, f); // 'RIFF'
  int size;
  fread(&size, 1, 4, f);
  char type[4];
  fread(type, 1, 4, f); // 'PAL '
  char dta[4];
  fread(dta, 1, 4, f); // 'data'
  int chuncksz;
  fread(&chuncksz, 1, 4, f);
  short palVersion, palEntries;
  fread(&palVersion, sizeof(short), 1, f);
  fread(&palEntries, sizeof(short), 1, f);
  for (int i = 0; i < palEntries; ++i) {
    unsigned char r, g, b, _;
    fread(&r, 1, 1, f);
    fread(&g, 1, 1, f);
    fread(&b, 1, 1, f);
    fread(&_, 1, 1, f);
    *(pal++) = r;
    *(pal++) = g;
    *(pal++) = b;
  }
  fclose(f);
}

// --------------------------------------------------------------

// returns p so that (1<<p) >= n
int justHigherPow2(int n)
{
  int  p2 = 0;
  bool isp2 = true;
  while (n > 0) {
    if (n > 1 && (n & 1)) {
      isp2 = false;
    }
    ++p2;
    n = n >> 1;
  }
  return isp2 ? p2 - 1 : p2;
}

// selects a pow2 size for a light map
int selectLmapPow2(int n)
{
  int  p2 = justHigherPow2(n);
  switch (p2) {
  case 0: case 1: case 2: case 3: return 3;
  default: return p2;
  }
  return p2;
}

// --------------------------------------------------------------

// computes the light map dimension from a face
v2i lmap_dimensions(const face_t *fc,const vector<v3f>& face,v3f& _p_ref)
{
  // read surface info
  surface_t srf;
  read(fc->texinfo_id, &srf);
  // CalcFaceExtents
  // https://github.com/fabiensanglard/Quake--QBSP-and-VIS/blob/e686204812f6464864e2959f9f57c1278409b70b/light/ltface.c#L172
  v3f s = to_v3f(srf.vectorS);
  v3f t = to_v3f(srf.vectorT);
  AAB<2> bx;
  for (auto p : face) {
    bx.addPoint(v2f(dot(p, s), dot(p, t)));
  }
  v2f pmin = v2f(
    16.0f * floor(bx.minCorner()[0] / 16.0f),
    16.0f * floor(bx.minCorner()[1] / 16.0f));
  _p_ref = s * pmin[0] + t * pmin[1];
  v2f test = v2f(dot(_p_ref, s), dot(_p_ref, t));
  int w = 1 + ceil(bx.maxCorner()[0] / 16.0f) - floor(bx.minCorner()[0] / 16.0f);
  int h = 1 + ceil(bx.maxCorner()[1] / 16.0f) - floor(bx.minCorner()[1] / 16.0f);
  return v2i(w, h);
}

// --------------------------------------------------------------

int numlightmaps = 0; // counts light maps

// a light map pack texture
typedef struct {
  uchar *pixels;
  int w, h;   // width and height of the pack
  int ti, tj; // next free tile
  int tw, th; // tile w and height
  int tex_id; // pack texture id once determined
} lmap_pack;

// all light map packs, by power of two of lightmap tile size
vector<vector<lmap_pack*> > lmap_packs;

// prepares a light map pack
lmap_pack *lmap_pack_pre(int sz,int tsz)
{
  lmap_pack *pack = new lmap_pack();
  pack->w         = pack->h = sz;
  pack->pixels    = new uchar[sz*sz];
  memset(pack->pixels, 0xff, sz * sz);
  pack->ti = pack->tj = 0;
  pack->tw = pack->th = tsz;
  pack->tex_id = -1;
  sl_assert(sz % pack->tw == 0);
  sl_assert(sz % pack->th == 0);
  return pack;
}

// checks if the pack is full
bool lmap_pack_full(lmap_pack *pack)
{
  return pack->tj >= pack->h;
}

// adds a light map tile to the pack
v2i lmap_pack_add(lmap_pack *pack,uchar *pixs,int w,int h)
{
  sl_assert(w <= pack->tw && h <= pack->th);
  sl_assert(!lmap_pack_full(pack));
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      float l = (float)pixs[i + j * w] / 255.0f;
      // l       = pow(l,0.9f);
      int il  = max(0,min(255, 32 + (int)floor(l*255.0f)));
      pack->pixels[(i + pack->ti) + (j + pack->tj) * pack->w]
        = il;
    }
  }
  v2i pos = v2i(pack->ti, pack->tj);
  pack->ti += pack->tw;
  if (pack->ti == pack->w) {
    pack->ti  = 0;
    pack->tj += pack->th;
  }
  return pos;
}

// diffuse around light maps to fill-in the padding, hack to avoid cracks
// in rendering as uvs are not precise anough to exactly align
bool lmap_pack_pad(lmap_pack *pack)
{
  bool done = true;
  uchar *tmp = new uchar[pack->w * pack->h];
  memcpy(tmp, pack->pixels, pack->w * pack->h);
  for (int j = 0; j < pack->h; ++j) {
    for (int i = 0; i < pack->w; ++i) {
      if (tmp[i + j * pack->h] == 0xff) {
        // average nieghbors
        int avg = 0;
        int n = 0;
        for (int y = max(0, j - 1); y <= min(pack->h - 1, j + 1); ++y) {
          for (int x = max(0, i - 1); x <= min(pack->w - 1, i + 1); ++x) {
            if (tmp[x + y * pack->h] != 0xff) {
              avg += tmp[x + y * pack->h];
              ++n;
            }
          }
        }
        if (n == 0) {
          pack->pixels[i + j * pack->h] = 0xff;
          done = false;
        } else {
          pack->pixels[i + j * pack->h] = avg / n;
        }
      }
    }
  }
  delete[](tmp);
  return done;
}

// saves a lightmap pack for visualization purposes
void lmap_pack_save(lmap_pack *pack,const char *name)
{
  t_image_nfo img;
  img.depth = 24;
  img.width = pack->w;
  img.height = pack->h;
  img.pixels = new uchar[pack->w * pack->h * 3];
  for (int i = 0; i < pack->w * pack->h; ++i) {
    img.pixels[i * 3 + 0] = pack->pixels[i];
    img.pixels[i * 3 + 1] = pack->pixels[i];
    img.pixels[i * 3 + 2] = pack->pixels[i];
  }
  SaveTGAFile((std::string(SRC_PATH) + std::string(name)).c_str(), &img);
  delete[](img.pixels);
}

// --------------------------------------------------------------

// light map info for a face
typedef struct {
  lmap_pack *pack;
  v2i        uv_pos;
  v3f        pref;
} lmap_nfo;

map<int, lmap_nfo > face_to_lmap; // stores light map info for faces

// extracts all lights maps for leaf l
void extractLightMaps(int l)
{
  /// get faces
  std::vector<vector<v3f> >  faces;
  std::vector<int>           face_ids;
  getLeafFaces(l, faces, face_ids);
  /// light maps
  for (int fi = 0; fi < face_ids.size(); ++fi) {
    int fid = face_ids[fi];
    if (face_to_lmap.count(fid)) {
      // already known
      continue;
    }
    face_t fc;
    read(fid, &fc);
    if (fc.lightmap > -1 && l > 0) {
      ++numlightmaps;
      v3f pref;
      v2i ldim = lmap_dimensions(&fc, faces[fi], pref);
      long lmap_start = fc.lightmap;
      long lmap_end = fc.lightmap + ldim[0] * ldim[1];
      // read lightmap
      uchar *lmap = new uchar[ldim[0] * ldim[1]];
      fseek(f, h.lightmaps.offset + lmap_start, SEEK_SET);
      fread(lmap, ldim[0] * ldim[1], 1, f);
      // add to pack
      int p2 = selectLmapPow2(tupleMax(ldim));
      if (lmap_packs[p2].empty()) {
        lmap_packs[p2].push_back(lmap_pack_pre(256, 1 << p2));
      } else if (lmap_pack_full(lmap_packs[p2].back())) {
        lmap_packs[p2].push_back(lmap_pack_pre(256, 1 << p2));
      }
      v2i pos = lmap_pack_add(lmap_packs[p2].back(), lmap, ldim[0], ldim[1]);
      sl_assert(face_to_lmap.count(fid) == 0);
      lmap_nfo nfo;
      nfo.pack = lmap_packs[p2].back();
      nfo.uv_pos = pos;
      nfo.pref = pref;
      face_to_lmap[fid] = nfo;
#if 0
      // save lightmap
      t_image_nfo img;
      img.depth = 24;
      img.width = ldim[0];
      img.height = ldim[1];
      img.pixels = new uchar[ldim[0] * ldim[1] * 3];
      for (int i = 0; i < ldim[0] * ldim[1]; ++i) {
        img.pixels[i * 3 + 0] = lmap[i];
        img.pixels[i * 3 + 1] = lmap[i];
        img.pixels[i * 3 + 2] = lmap[i];
      }
      SaveTGAFile((std::string(SRC_PATH) + sprint("lmaps\\%04d.tga", fid)).c_str(), &img);
      delete[](img.pixels);
#endif
      delete[](lmap);
    }
  }
}

// --------------------------------------------------------------

map<int, int> face_usage;    // tracks face usage for debugging purposes
int           max_verts = 0; // max num vertices in a leaf

// packs a leaf in the data pack
void packLeaf(
  FILE        *pack,
  int          l,
  vector<v3f>&            _global_uniquen,
  vector<pair<v4f,v4f> >& _global_uniques,
  int&         _vis_first)
{
  /// get faces
  std::vector<vector<v3f> >  faces;
  std::vector<int>           face_ids;
  getLeafFaces(l, faces, face_ids);
  /// texture ids
  vector<int>      face_texids;
  std::vector<int> faces_tvc_idx;
  faces_tvc_idx.resize(faces.size());
  int fidx = 0;
  for (auto fid : face_ids) {
    ++ face_usage[fid];
    face_t fc;
    read(fid, &fc);
    surface_t tnfo;
    read(fc.texinfo_id, &tnfo);
    face_texids.push_back(tnfo.texture_id);
    // surface test
    pair<v4f, v4f> s = make_pair(
      v4f(to_v3f(tnfo.vectorS), tnfo.distS),
      v4f(to_v3f(tnfo.vectorT), tnfo.distT)
      );
    int idx = search_surface(s, _global_uniques);
    if (idx == -1) {
      idx = _global_uniques.size();
      _global_uniques.push_back(s);
    }
    faces_tvc_idx[fidx] = idx;
    ++fidx;
  }
  /// compute face normal/surface
  std::vector<int> faces_nrm_idx;
  faces_nrm_idx.resize(faces.size());
  fidx = 0;
  for (const auto& f : faces) {
    if (f.size() < 3) continue;
    v3f n = face_normal(f);
    int idx = search_normal(n, _global_uniquen);
    if (idx == -1) {
      idx = _global_uniquen.size();
      _global_uniquen.push_back(n);
    }
    faces_nrm_idx[fidx] = idx;
    ++fidx;
  }
  /// merge vertices
  vector<v3f>  uniquev;
  int numins = 0;
  for (const auto& f : faces) {
    for (const auto& p : f) {
      int idx = search_vertex(p, uniquev);
      if (idx == -1) {
        uniquev.push_back(p);
      }
      ++numins;
    }
  }
  max_verts = max(max_verts, (int)uniquev.size());
  /// rewrite faces as lists of vertex ids
  std::vector<vector<int> > ifaces;
  ifaces.reserve(faces.size());
  int num_indices = 0;
  for (const auto& f : faces) {
    ifaces.push_back(vector<int>());
    ifaces.back().reserve(f.size());
    for (const auto& p : f) {
      ifaces.back().push_back(search_vertex(p, uniquev));
      ++num_indices;
    }
  }
  /// visibility list
  vector<int> vis;
  getLeafVislist(l, vis);
  int vis_len = (int)vis.size();
#if 0
  // print info
  printf("leaf %d, %d vertices (/%d), %d faces, vis first: %d, len:%d\n",
    l, uniquev.size(), numins, faces.size(), _vis_first,vis_len);
#endif
  /// output
  // -> bounding box
  dleaf_t llf;
  read(l, &llf);
  short v;
  v = llf.bound.min_x * scale;
  fwrite(&v, sizeof(short), 1, pack);
  v = llf.bound.min_z * scale; // swap z<->y
  fwrite(&v, sizeof(short), 1, pack);
  v = llf.bound.min_y * scale;
  fwrite(&v, sizeof(short), 1, pack);
  v = llf.bound.max_x * scale;
  fwrite(&v, sizeof(short), 1, pack);
  v = llf.bound.max_z * scale; // swap z<->y
  fwrite(&v, sizeof(short), 1, pack);
  v = llf.bound.max_y * scale;
  fwrite(&v, sizeof(short), 1, pack);
  // -> vis list start and length
  fwrite(&_vis_first, sizeof(int), 1, pack);
  fwrite(&vis_len, sizeof(int), 1, pack);
  // -> vertices
  int numv = (int)uniquev.size();
  fwrite(&numv, sizeof(int), 1, pack);
  for (int v = 0; v < uniquev.size(); ++v) {
    v3s iv = v3s(scale * uniquev[v]);
    coord_swap(iv);
    fwrite(&iv[0], sizeof(short), 1, pack);
    fwrite(&iv[1], sizeof(short), 1, pack);
    fwrite(&iv[2], sizeof(short), 1, pack);
  }
  // -> faces
  //    face defs
  int numf = (int)faces.size();
  fwrite(&numf, sizeof(int), 1, pack);
  short start = 0;
  fidx = 0;
  for (const auto& f : ifaces) {
    fwrite(&start, sizeof(short), 1, pack); // start index
    short sz     = (short)f.size();
    fwrite(&sz, sizeof(short), 1, pack);    // number of indices
    short nrm    = faces_nrm_idx[fidx];
    fwrite(&nrm, sizeof(short), 1, pack);   // index of normal
    short tvc    = faces_tvc_idx[fidx];
    fwrite(&tvc, sizeof(short), 1, pack);   // index of texturing info
    short texid  = 2 + face_texids[fidx];
    fwrite(&texid, sizeof(short), 1, pack); // texture id
    if (face_to_lmap.count(face_ids[fidx])) {
      const lmap_nfo& lmapnfo = face_to_lmap.at(face_ids[fidx]);
      short lmapid = 2 + lmapnfo.pack->tex_id;
      fwrite(&lmapid, sizeof(short), 1, pack); // lmap texture
      sl_assert(lmapnfo.uv_pos[0] >= 0 && lmapnfo.uv_pos[0] < 256
            && lmapnfo.uv_pos[1] >= 0 && lmapnfo.uv_pos[1] < 256);
      unsigned short lmap_uv = (lmapnfo.uv_pos[0] & 255) | ((lmapnfo.uv_pos[1] & 255) << 8);
      fwrite(&lmap_uv, sizeof(unsigned short), 1, pack); // lmap uv
      v3s pref = v3s(lmapnfo.pref * scale);
      coord_swap(pref);
      fwrite(&pref[0], sizeof(short), 1, pack);
      fwrite(&pref[1], sizeof(short), 1, pack);
      fwrite(&pref[2], sizeof(short), 1, pack);
    } else {
      /// TODO: store info for the no-lightmap case
      short zeros[] = { 0,0,0,0,0 };
      fwrite(&zeros, sizeof(short), 5, pack);
    }
    start += f.size();
    ++fidx;
  }
  //    indices
  for (const auto& f : ifaces) {
    for (const auto& i : f) {
      fwrite(&i, sizeof(int), 1, pack);
    }
  }
  // increment first visibility entry
  _vis_first += vis_len;
}

// --------------------------------------------------------------

void remap(int& _c) // c is a 6 bit color component
{
  // remap color to make them brighter
  float f = (float)_c / 64.0f;
  f  = pow(f, 0.65f);
  _c = min(63.0f, max(0.0f, f * 64.0f));
}

// --------------------------------------------------------------

void packTextures(FILE *pack)
{
  /// produce data pack
  // get palette
  unsigned char pal[768];
  load_palette(SRC_PATH "palette.pal", pal);
  // get texture header
  fseek(f, h.miptex.offset, SEEK_SET);
  long numtex;
  fread(&numtex, sizeof(long), 1, f);
  vector<long> offsets;
  offsets.resize(numtex);
  fread(&offsets[0], sizeof(long), numtex, f);
  // 0-entry is all zeros
  for (int i = 0; i < 8; ++i) {
    unsigned char null = 0;
    fwrite(&null, 1, 1, pack);
  }
  // give an id to lightmap packs
  int lid = 0;
  vector<lmap_pack*> lmapks;
  for (auto pks : lmap_packs) {
    for (auto pk : pks) {
      pk->tex_id = numtex + (lid++);
      lmapks.push_back(pk);
    }
  }
  int numlmaps = lid;
  // write texture table
  int tex_addr = (2 << 20) /*2MB offset*/ + 8 * (2 + numtex + numlmaps); // first texture address
  //                                            ^^ skip zero, one is full white for rendering lmaps only
  // full white debug texture (16x16)
  {
    // write address
    unsigned char b;
    b = tex_addr & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 8) & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 16) & 255;
    fwrite(&b, 1, 1, pack);
    // size byte (hp2,wp2)
    int wp2 = 4;
    int hp2 = 4;
    b = wp2 | (hp2 << 4);
    fwrite(&b, 1, 1, pack);
    b = 0;
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    // next
    tex_addr += 16 * 16;
  }
  // game textures
  for (int t = 0; t < numtex; ++t) {
    // read nfo
    miptex_t tnfo;
    fseek(f, h.miptex.offset + offsets[t], SEEK_SET);
    fread(&tnfo, sizeof(tnfo), 1, f);
    if (tnfo.name[0] == '\0') {
      // a strange entry in e1m2
      tnfo.width = tnfo.height = 0;
    }
    // printf("%d, %s %dx%d\n", t, tnfo.name, tnfo.width, tnfo.height);
    // write address
    unsigned char b;
    b = tex_addr & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 8) & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 16) & 255;
    fwrite(&b, 1, 1, pack);
    // size byte (hp2,wp2)
    int wp2 = justHigherPow2(tnfo.width);
    int hp2 = justHigherPow2(tnfo.height);
    b = wp2 | (hp2 << 4);
    fwrite(&b, 1, 1, pack);
    b = 0;
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    // next
    tex_addr += (1<< justHigherPow2(tnfo.width)) * (1 << justHigherPow2(tnfo.height));
  }
  // lightmap packs
  for (auto pk : lmapks) {
    // write address
    unsigned char b;
    b = tex_addr & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 8) & 255;
    fwrite(&b, 1, 1, pack);
    b = (tex_addr >> 16) & 255;
    fwrite(&b, 1, 1, pack);
    // size byte (hp2,wp2)
    int wp2 = justHigherPow2(pk->w);
    int hp2 = justHigherPow2(pk->h);
    b = wp2 | (hp2 << 4);
    fwrite(&b, 1, 1, pack);
    b = 0;
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    fwrite(&b, 1, 1, pack);
    // next
    tex_addr += pk->w * pk->h;
  }
  // write data
  // -> white texture
  uchar white[16 * 16];
  memset(white, 254, 16*16);
  fwrite(&white[0], 1, sizeof(white), pack);
  // -> game textures
  for (int t = 0; t < numtex; ++t) {
    // read nfo
    miptex_t tnfo;
    fseek(f, h.miptex.offset + offsets[t], SEEK_SET);
    fread(&tnfo, sizeof(tnfo), 1, f);
    if (tnfo.name[0] == '\0') {
      // a strange entry in e1m2
      continue;
    }
    // fprintf(stderr, "%d, %s %dx%d\n", t, tnfo.name, tnfo.width, tnfo.height);
    // read pixels
    vector<unsigned char> pixs;
    pixs.resize(tnfo.width * tnfo.height);
    fseek(f, h.miptex.offset + offsets[t] + tnfo.offset1, SEEK_SET);
    fread(&pixs[0], 1, pixs.size(), f);
#if 0
    // add borders for debugging
    for (int j = 0; j < tnfo.height; ++j) {
      int i = 0;
      pixs[i + j * tnfo.width] = 111;
      i = tnfo.width - 1;
      pixs[i + j * tnfo.width] = 244;
    }
#endif
    // make padded version
    vector<unsigned char> padded;
    padded.resize((1 << justHigherPow2(tnfo.width)) * (1 << justHigherPow2(tnfo.height)));
    for (int j = 0; j < (1 << justHigherPow2(tnfo.height)); ++j) {
      for (int i = 0; i < (1 << justHigherPow2(tnfo.width)); ++i) {
        padded[i + j * (1 << justHigherPow2(tnfo.width))] = pixs[(i% tnfo.width) + (j% tnfo.height) * tnfo.width];
      }
    }
    // write
    fwrite(&padded[0], 1, padded.size(), pack);
#if 0
    // save as image for visualization
    t_image_nfo img;
    img.depth = 24;
    img.width = (1 << justHigherPow2(tnfo.width));
    img.height = (1 << justHigherPow2(tnfo.height));
    img.pixels = new uchar[(1 << justHigherPow2(tnfo.width)) * (1 << justHigherPow2(tnfo.height)) * 3 * sizeof(uchar)];
    for (int i = 0; i < (1 << justHigherPow2(tnfo.width)) * (1 << justHigherPow2(tnfo.height)); ++i) {
      img.pixels[i * 3 + 0] = pal[padded[i] * 3 + 0];
      img.pixels[i * 3 + 1] = pal[padded[i] * 3 + 1];
      img.pixels[i * 3 + 2] = pal[padded[i] * 3 + 2];
    }
    SaveTGAFile((std::string(SRC_PATH) + tnfo.name + ".tga").c_str(), &img);
    delete[](img.pixels);
#endif
  }
  // -> light maps
  for (auto pk : lmapks) {
    fwrite(pk->pixels, 1, pk->w*pk->h, pack);
  }
  // write palette
  {
    FILE *fpal = fopen(SRC_PATH "/../../build/palette666.si", "w");
    sl_assert(fpal != NULL);
    fprintf(fpal, "$$palette666 = '");
    for (int c = 0; c < 256; ++c) {
      int r6 = pal[c * 3 + 2] >> 2;
      int g6 = pal[c * 3 + 1] >> 2;
      int b6 = pal[c * 3 + 0] >> 2;
      // remap(r6); remap(g6); remap(b6);
      int rgb_666 = r6 | (g6 << 6) | (b6 << 12);
      fprintf(fpal, "%d,", rgb_666);
    }
    fprintf(fpal, "'\n");
    fclose(fpal);
  }
}

// --------------------------------------------------------------

void packBSP(FILE *pack,long &_offset_bsp_nodes,long &_offset_bsp_planes)
{
  /// rewrite the BSP to be more compact and integer
  typedef struct {
    u_short     plane_id;
    u_short     front;
    u_short     back;
    bboxshort_t box;
  } t_my_node;
  typedef struct {
    short nx, ny, nz;
    int   dist;
  } t_my_plane;
  vector<t_my_node> nodes;
  int num_nodes = (int)h.nodes.size / (int)sizeof(node_t);
  for (int n = 0; n < num_nodes; ++n) {
    node_t nd;
    read(n, &nd);
    nodes.push_back(t_my_node());
    nodes.back().back = nd.back;
    nodes.back().front = nd.front;
    nodes.back().plane_id = (u_short)nd.plane_id;
    // scale and swap
    nodes.back().box.min_x = (short)(nd.box.min_x * scale);
    nodes.back().box.min_y = (short)(nd.box.min_z * scale); // swap
    nodes.back().box.min_z = (short)(nd.box.min_y * scale);
    nodes.back().box.max_x = (short)(nd.box.max_x * scale);
    nodes.back().box.max_y = (short)(nd.box.max_z * scale); // swap
    nodes.back().box.max_z = (short)(nd.box.max_y * scale);
  }
  vector<t_my_plane> planes;
  int num_planes = (int)h.planes.size / (int)sizeof(plane_t);
  for (int p = 0; p < num_planes; ++p) {
    plane_t pl;
    read(p, &pl);
    planes.push_back(t_my_plane());
    planes.back().dist = pl.dist * scale;
    v3i nrm = v3i(to_v3f(pl.normal) * 256.0f);
    coord_swap(nrm);
    planes.back().nx = nrm[0];
    planes.back().ny = nrm[1];
    planes.back().nz = nrm[2];
  }
  // write bsp tree
  _offset_bsp_nodes  = (2 << 20) /*2MB offset*/ + ftell(pack);
  fwrite(&nodes[0], sizeof(t_my_node), nodes.size(), pack);
  _offset_bsp_planes = (2 << 20) /*2MB offset*/ + ftell(pack);
  fwrite(&planes[0], sizeof(t_my_plane), planes.size(), pack);
}

// --------------------------------------------------------------

int packVisList(FILE *pack)
{
  int maxvis = 0;
  int totvis = 0;
  int numleaves = h.leaves.size / sizeof(dleaf_t);
  for (int l = 0; l < numleaves; ++l) {
    vector<int> vis;
    getLeafVislist(l, vis);
    maxvis = max(maxvis, (int)vis.size());
    for (auto vl : vis) {
      unsigned short s = (unsigned short)vl;
      fwrite(&vl, sizeof(unsigned short), 1, pack);
    }
    totvis += vis.size();
  }
  return maxvis;
}

// --------------------------------------------------------------

int main(int argc, const char **argv)
{
  // open BSP file
  fopen_s(&f, SRC_PATH "/../" MAP, "rb");
  if (f == NULL) {
    fprintf(stderr, "\n\n[error] cannot open the bsp file, please place "
                    MAP " in the q5k directory.\n\n\n");
    return -1;
  }
  // read header
  fread(&h, sizeof(h), 1, f);
  // some info
  fprintf(stderr, "version:\t%04x\n",h.version);
  fprintf(stderr, "entities:\t@%04x %6d bytes\n", h.entities.offset,h.entities.size);
  fprintf(stderr, "planes:\t@%04x %6d bytes (num:%d)\n", h.planes.offset, h.planes.size, (int)h.nodes.size / (int)sizeof(plane_t));
  fprintf(stderr, "miptex:\t@%04x %6d bytes\n", h.miptex.offset, h.miptex.size);
  fprintf(stderr, "vertices:\t@%04x %6d bytes\n", h.vertices.offset, h.vertices.size);
  fprintf(stderr, "visilist:\t@%04x %6d bytes\n", h.visilist.offset, h.visilist.size);
  fprintf(stderr, "nodes:\t@%04x %6d bytes (num:%d)\n", h.nodes.offset, h.nodes.size, (int)h.nodes.size/ (int)sizeof(node_t));
  fprintf(stderr, "texinfo:\t@%04x %6d bytes (num:%d)\n", h.texinfo.offset, h.texinfo.size, (int)h.nodes.size / (int)sizeof(surface_t));
  fprintf(stderr, "faces:\t@%04x %6d bytes\n", h.faces.offset, h.faces.size);
  fprintf(stderr, "lightmaps:\t@%04x %6d bytes\n", h.lightmaps.offset, h.lightmaps.size);
  fprintf(stderr, "clipnodes:\t@%04x %6d bytes\n", h.clipnodes.offset, h.clipnodes.size);
  fprintf(stderr, "leaves:\t@%04x %6d bytes\n", h.leaves.offset, h.leaves.size);
  fprintf(stderr, "lface:\t@%04x %6d bytes\n", h.lface.offset, h.lface.size);
  fprintf(stderr, "edges:\t@%04x %6d bytes\n", h.edges.offset, h.edges.size);
  fprintf(stderr, "ledges:\t@%04x %6d bytes\n", h.ledges.offset, h.ledges.size);
  fprintf(stderr, "models:\t@%04x %6d bytes\n", h.models.offset, h.models.size);
  fprintf(stderr, "\n");
  // model 0
  model_t m;
  read(0, &m);
  fprintf(stderr, "model 0: numleafs %d\n", m.numleafs);
  fprintf(stderr, "model 0: face_id %d face_num %d\n", m.face_id, m.face_num);
  fprintf(stderr, "model 0: origin %f %f %f\n", m.origin.x,m.origin.y,m.origin.z);
  fprintf(stderr, "model 0: node_id0 %d\n", m.node_id0);
  // ----------------------------------------------------------------
  // produce data pack
  // ----------------------------------------------------------------
  FILE *pack = fopen(SRC_PATH "/../../build/quake.img", "wb");
  sl_assert(pack != NULL);
  ofstream hd(SRC_PATH "/../q.h");
  // ----------------------------------------------------------------
  /// produce light maps
  int numleaves = h.leaves.size / sizeof(dleaf_t);
  for (int p2 = 0; p2 <= 5; p2++) {
    lmap_packs.push_back(vector<lmap_pack*>());
  }
  for (int l = 0; l < numleaves; ++l) {
    extractLightMaps(l);
  }
  printf("num lightmaps: %d\n", numlightmaps);
  int p = 0;
  for (int p2 = 0; p2 < lmap_packs.size(); ++p2) {
    for (auto pk : lmap_packs[p2]) {
      while (!lmap_pack_pad(pk)) { } // diffuse
      // lmap_pack_save(pk, sprint("lmaps\\pack%02d_%02d.tga", p2, p++));
    }
  }
  // ----------------------------------------------------------------
  /// pack all textures
  packTextures(pack);
  // ----------------------------------------------------------------
  /// pack BSP tree
  long offset_bsp_nodes, offset_bsp_planes;
  packBSP(pack, offset_bsp_nodes, offset_bsp_planes);
  // ----------------------------------------------------------------
  /// pack leaves
  vector<int>  leaf_offsets;
  vector<v3f>  global_uniquen;
  vector<pair<v4f, v4f> > global_uniques;
  int vis_first = 0;
  for (int l = 0; l < numleaves; ++l) {
    leaf_offsets.push_back((2 << 20) /*2MB offset*/ + ftell(pack) );
    packLeaf(pack, l, global_uniquen, global_uniques, vis_first);
  }
  // add one more to tag end
  leaf_offsets.push_back((2 << 20) /*2MB offset*/ + ftell(pack));
  /// store leaf offsets
  long offset_leaf_offsets = (2 << 20) /*2MB offset*/ + ftell(pack);
  fwrite(&leaf_offsets[0], sizeof(int), leaf_offsets.size(), pack);
  // max leaf size
  int max_leaf_size = 0;
  for (int i = 0; i < (int)leaf_offsets.size()-1; ++i) {
    max_leaf_size = max(max_leaf_size, leaf_offsets[i+1]- leaf_offsets[i]);
  }
  // ----------------------------------------------------------------
  /// pack visibility list
  long offset_vislist = (2 << 20) /*2MB offset*/ + ftell(pack);
  int maxvis_len = packVisList(pack);
  // ----------------------------------------------------------------
  fclose(pack);
  // ----------------------------------------------------------------
  /// write normals and texturing vectors in header
  // normals
  hd << "#define n_normals " << global_uniquen.size() << "\n";
  hd << "p3d normals[] = {\n";
  for (auto nrm : global_uniquen) {
    v3i i_n = v3i(-nrm * 256.0f);
    coord_swap(i_n);
    hd << '{'
      << i_n[0] << ',' << i_n[1] << ',' << i_n[2] << ','
      << "},\n";
  }
  hd << "};\n";
  // texturing vectors
  hd << "typedef struct { p3d vecS; short distS; p3d vecT; short distT; } t_texvecs;\n";
  hd << "#define n_texvecs " << global_uniques.size() << "\n";
  hd << "t_texvecs texvecs[] = {\n";
  for (auto srf : global_uniques) {
    v3i i_s = v3i(v3f(srf.first) * 256.0f);
    coord_swap(i_s);
    v3i i_t = v3i(v3f(srf.second) * 256.0f);
    coord_swap(i_t);
    hd << '{'
      << '{' << i_s[0] << ',' << i_s[1] << ',' << i_s[2] << "}," << (short)(srf.first[3]  * scale) << ','
      << '{' << i_t[0] << ',' << i_t[1] << ',' << i_t[2] << "}," << (short)(srf.second[3] * scale) << ','
      << "},\n";
  }
  hd << "};\n";
  /// write offsets in header
  hd << "#define o_bsp_nodes    " << offset_bsp_nodes  << '\n';
  hd << "#define o_bsp_planes   " << offset_bsp_planes << '\n';
  hd << "#define o_vislist      " << offset_vislist << '\n';
  hd << "#define o_leaf_offsets " << offset_leaf_offsets << '\n';
  /// write start viewpos in header
  v3i pview = v3i(scale * view_pos);
  coord_swap(pview);
  hd << "p3d view = {"
    << pview[0] << ',' << pview[1] << ',' << pview[2]
    << "};\n";
  /// write max vis list length
  hd << "#define n_max_vislen " << maxvis_len << '\n';
  hd << "#define n_max_verts " << max_verts << '\n';
  hd << "#define n_max_leaf_size " << max_leaf_size << '\n';
  /// write additional definitions in header
  hd << "typedef struct { unsigned short plane_id; unsigned short front; unsigned short back; aabb box; } t_my_node;\n";
  hd << "typedef struct { short nx, ny, nz; int dist; } t_my_plane;\n";
  // ----------------------------------------------------------------
  // close bsp file
  fclose(f);
  return 0;
}

// --------------------------------------------------------------
