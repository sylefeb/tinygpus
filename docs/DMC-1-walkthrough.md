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

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=206-211) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // RAM for CPU (SPRAM)
  bram_port_io mem;
  bram_spram_32bits bram_ram(
    pram               <:> mem
  );
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The CPU ([my ice-v-dual CPU](https://github.com/sylefeb/Silice/blob/master/projects/ice-v/README.md))

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=213-219) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // CPU
  uint32 user_data(0);
  rv32i_cpu cpu(
    user_data        <:: user_data,
    mem              <:> mem,
  );
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU (!)

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=221-225) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // GPU
  texmem_io     txm_io;
  DMC_1_gpu gpu(txm          <:>  txm_io,
                screen_ready <:   screen_ctrl.ready);
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU texture memory

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=272-278) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // texture memory
$if MCH2022 or (SIMULATION and SIMUL_QPSRAM) then
    qpsram_ram txm<@clock2x,!rst2x,reginputs> (
      ram_clk  :> ram_clk,  ram_csn :>  ram_csn,
      ram_io0 <:> ram_io0,  ram_io1 <:> ram_io1,
      ram_io2 <:> ram_io2,  ram_io3 <:> ram_io3,
    );
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU command queue

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=320-322) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // command queue
  command_queue cmdq(current :> gpu.command);
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The screen controller and screen driver

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=227-237) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // screen display
  uint1 screen_valid(0);    uint1 screen_ready(0);
  uint1 screen_send_dc(0);  uint8 screen_send_byte(0);
  screen_controller screen_ctrl(
    screen_valid :> screen_valid,
    screen_ready <: screen_ready,
    send_dc      :> screen_send_dc,
    send_byte    :> screen_send_byte,
  );
  // ...
```
<!-- MARKDOWN-AUTO-DOCS:END -->
