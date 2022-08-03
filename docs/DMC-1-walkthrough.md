# DMC-1 design walkthrough

The DMC-1 design is in the file [`dmc-1.si`](../hardware/GPUs/dmc-1/dmc-1.si) (it contains many comments).

In the same directory there are a number of helper units which are not strictly part of the DMC-1 but are provided to help assemble a SOC: a [fifo command queue](../hardware/GPUs/dmc-1/command_queue.si) to store draw calls before sending them to the GPU, and screen drivers for a [SPI](../hardware/GPUs/dmc-1/spi_screen.si) and [parallel](../hardware/GPUs/dmc-1/parallel_screen.si) interface.

These are used to assemble a complete SOC including a RISC-V processor in [`soc-ice40-dmc-1-risc_v.si`](../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si).

## The SOC

> What's a SOC? This means *System On a Chip* and is basically the assembled system
> that forms the hardware running our demos.

Let's start from the top and look at the main components of the SOC running the demos.
The SOC instantiates six major components:

- The CPU RAM, using the UP5K on-chip 128KB SPRAM

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=206-211) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
  // draw command type
  uint1  wall     <:: in_command[30,2] == 2b00;   // wall?
  uint1  plane    <:: in_command[30,2] == 2b01;   // plane? (perspective span)
  uint1  terrain  <:: in_command[30,2] == 2b10;   // terrain?
  uint1  param    <:: in_command[30,2] == 2b11;   // parameter?
  // on a param decode which data is sent
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The CPU ([my ice-v-dual CPU](https://github.com/sylefeb/Silice/blob/master/projects/ice-v/README.md))

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=213-219) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
  uint1  planeA   <:: param & (in_command[62,2] == 2b10);
  uint1  uv_offs  <:: param & (in_command[62,2] == 2b01);
  uint1  set_vwz  <:: param & (in_command[62,2] == 2b11);
  // other
  uint1  pickh    <:: terrain & in_command[63,1]; // pick terrain height?

  // ---- drawing status
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU (!)

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=221-225) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
  uint8  current(0);     // current y pos along span
  uint8  end    (0);     // ending y value
  uint1  pickh_done(0);  // picking done
  uint1  current_done <:: (current >= end); // column end reached
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU texture memory

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=271-278) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
  // rotating bit vector implementing the per-pixel cycle, such that
  // the pixel computation is done when the MSB is 1
  uint$delay_bit+1$ smplr_delay(0);
  uint1  start(0);     // start fetching a span
  uint3  skip(0);      // do not increment along span and disable pixel writes

  // take into account the various delays for texel fetch
  // - LSB are most delayed
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU command queue

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=320-322) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
    // (sampler works in parallel)
    switch ({terrain,state})
    {
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The screen controller and screen driver

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=227-237) -->
<!-- The below code snippet is automatically added from ../hardware/GPUs/dmc-1/dmc-1.si -->
```c
  int24  dot_u(0);
  int24  dot_v(0);
  int24  dot_ray(0);
  int24  ded(0);
  int10  ny_inc(0);
  int10  uy_inc(0);
  int10  vy_inc(0);
  int32  ray_t(0);
  int24  u_offset(0);
  int24  v_offset(0);
```
<!-- MARKDOWN-AUTO-DOCS:END -->
