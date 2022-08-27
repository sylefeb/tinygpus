## DMC-1 GPU API

Draw commands are made through a basic API; each call requires sending two 32 bits words for a total of 64 bits per command.

The GPU allows to draw different types of spans: wall, plane, terrain and a fourth type that is not a span but a call to specify rendering parameters.

The CPU side API is [here](../../../software/api/api.c).

For instance, to draw a wall from `yf` to `yc` (vertical screen coordinates, `yf <= yc`, if equal 1 pixel is drawn) using light level `seclight` (0-15) and texture id `tid`, at a distance `d` from the user (`d` is the texture coordinate increment for a wall), starting from texture coordinate `tex_v` (initial value of vertical texture coordinate, varies along screen height) and `tc_u` (horizontal texture coordinate, varies along screen width but constant in a column span), use the following call:

```c
  col_send(
    COLDRAW_WALL(d,tex_v,tc_u),
    COLDRAW_COL(tid, yf,yc, seclight) | WALL
  );
```

## Draw Commands encoding

Two 32 bits words: Tex1, Tex0

Draw command types: wall (00), plane (01), terrain (10), parameter (11)

___
### **Tex1** tag, texture id, column start/end

___
__wall, plane, terrain__ (`tag != 2b11`)

|  31-30 (2) |  29-26 (4) |  25-18 (8) |  17-10 (8) | 0-9 (10)   |
|------------|------------|------------|------------|------------|
| tag        |  light     |  col end   | col start  | texture id |

> col_end >= col_start or command is ignored

___
__parameter__ (`tag == 2b11`)

|  31-30 (2) | 29-1 (29)                     |  0 (1)      |
|------------|-------------------------------|-------------|
| 2b11       | usage based on `tag2` in Tex0 |  end of col |

- __planeA data__ (`tag == 2b11` *and* `tag2 == 10`)

|  31-30 (2) | 29 (1)  | 28-15 (14) | 14-1 (14) | 0 (1)      |
|------------|---------|------------|-----------|------------|
| 2b11       | unused  |    dv      |  du       | end of col |

- __uv offset__ (`tag == 2b11` *and* `tag2 == 01`)

|  31-30 (2) | 29-24 (4) | 25 (1)          | 24-1 (24) | 0 (1)      |
|------------|-----------|-----------------|-----------|------------|
| 2b11       | unused    | lightmap enable | u_offset  | end of col |

__end of column__ (`tag == 2b11` and `end of column == 1`)

Note: the command is not passed further and thus there is no confusion with
the other cases.

___
### **Tex0** depending on `tag`

___
__Wall__ (`tag == 2b00`)

| 31-24  (8) | 23-16 (8) | 15-0 (16)  |
|------------|-----------|------------|
| u_init     | v_init    | wall y     |

___
__Plane__ (`tag == 2b01`)

| 31-16 (16)  | 15-0 (16)  |
|-------------|------------|
| dot_ray     |  ded       |

___
__Terrain__ (`tag == 2b10`)

| 31  (1) | 30-16 (8) | 15-0 (16)  |
|---------|-----------|------------|
| pick    | end dist  | start dist |

(needs to have columns sin/cos set before, see below)

___
__Parameter__ (`tag == 2b11`) (30 bits max)

| 31-30 (2)  | 29-0 (30)  |
|------------|------------|
| `tag2`     | `data`     |

- `tag2==00`, `data` is column sine/cosine data

| 29 (1) | 28-14 (14) | 13-0 (14)  |
|--------|------------|------------|
| unused | sin        | cos        |

- `tag2==01`, `data` is UV offset data (also uses `u_offset` from Tex1)

| 29-24 (6) | 23-0 (24)  |
|-----------|------------|
| unused    | v_offset   |

- `tag2==10`, `data` is plane A data (also uses `dv`,`du` from Tex1)

| 29-20 (10) | 19-10 (10)  | 9-0 (10)  |
|------------|------------|------------|
|  vy        |  uy        |  ny        |

- `tag2==11`, terrain view height

| 31-16 (16) | 15-0 (16)  |
|------------|------------|
|  unused    |  view_z    |
