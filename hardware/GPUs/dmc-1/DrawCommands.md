## DMC-1 GPU API

Draw commands are made through a basic API; each call requires sending two 32 bits words for a total of 64 bits per command.

The GPU allows to draw different types of spans: wall, perspective, terrain and a fourth type that is not a span but a call to specify rendering parameters.

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

Draw command types: wall (00), perspective (01), terrain (10), parameter (11)

**Tex1** command tag, texture id, column start/end

- `tag != 2b11` (wall, perspective, terrain)

|  31-30 (2) |  29-26 (4) |  25-18 (8) |  17-10 (8) | 0-9 (10)   |
|------------|------------|------------|------------|------------|
| tag        |  light     |  col end   | col start  | texture id |

> col_end > col_start or command is ignored, unless end of col

- `tag == 2b11` (parameter)

|  31-30 (2) | 29(1)  | 28-15 (14) | 14-1 (14) | 0 (1)      |
|------------|--------|------------|-----------|------------|
| 2b11       | unused |    dv      |  du       | end of col |

> If `tag == 2b11` and `end of column == 1`, the command is not
passed further and thus there is no confusion with the perspective A data
case below)

**Tex0** depending on `tag`

- Wall (`tag == 2b00`)

| 31-24  (8) | 23-16 (8) | 15-0 (16)  |
|------------|-----------|------------|
| u_init     | v_init    | wall y     |

- perspective (`tag == 2b01`)

| 31-16 (16)  | 15-0 (16)  |
|-------------|------------|
| dot_ray     |  ded       |

- Terrain (`tag == 2b10`)

| 31  (1) | 30-16 (8) | 15-0 (16)  |
|---------|-----------|------------|
| pick    | end dist  | start dist |

(needs to have columns sin/cos set before, see below)

- parameter (`tag == 2b11`) (30 bits max)

| 31-30 (2)  | 29-0 (30)  |
|------------|------------|
| `tag2`     | `data`     |

 `tag2==00`, `data` is column sine/cosine data

| 28-14 (14) | 13-0 (14)  |
|------------|------------|
| sin        | cos        |

 `tag2==01`, `data` is UV offset data

| 28-14 (14) | 13-0 (14)  |
|------------|------------|
| v          | u          |

 `tag2==10`, `data` is perspective A data
(also uses `dv`,`du` from Tex1)

| 29-20 (10) | 19-10 (10)  | 9-0 (10)  |
|------------|------------|------------|
|  vy        |  uy        |  ny        |

 `tag2==11`, terrain view height

| 31-16 (16) | 15-0 (16)  |
|------------|------------|
|  unused    |  view_z    |
