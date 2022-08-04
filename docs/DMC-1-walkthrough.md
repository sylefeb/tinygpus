# DMC-1 design and RISC-V SOC walkthrough

The DMC-1 design is in the file [`dmc-1.si`](../hardware/GPUs/dmc-1/dmc-1.si) (it contains many comments).

In the same directory there are a number of helper units which are not strictly part of the DMC-1 but are provided to help assemble a SOC: a [fifo command queue](../hardware/GPUs/dmc-1/command_queue.si) to store draw calls before sending them to the GPU, and screen drivers for a [SPI](../hardware/GPUs/dmc-1/spi_screen.si) and [parallel](../hardware/GPUs/dmc-1/parallel_screen.si) interface.

These are used to assemble a complete SOC including a RISC-V processor in [`soc-ice40-dmc-1-risc_v.si`](../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si).

## Part I - The SOC

> What's a SOC? This means *System On a Chip* and is basically the assembled system
> that forms the hardware running our demos.

Let's start from the top and look at the main components of the SOC running the demos.
The SOC instantiates six major components:

- The CPU RAM, using the UP5K on-chip 128KB SPRAM and a small BRAM for booting

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

- The GPU

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

Overall the CPU is the sole owner of the RAM, the GPU is the sole owner of the
texture memory and screen controller. However, the CPU has a bypass access
to both the screen and texture memory for initialization. The CPU should not
use this access while the GPU is drawing (this would not work properly).

### CPU memory and boot sequence

The CPU is directly plugged to the RAM with a binding to the group `mem`,
see `<:> mem` on both the RAM and CPU above. `mem` is declared as `bram_port_io mem`,
a group of variables for interfacing with BRAM type memories.

The [`bram_spram_32bits`](../hardware/SOCs/ice40-dmc-1/bram_spram_32bits.si) unit
combines a BRAM (that can be initialized, but we have little of it) with the
larger SPRAM (128 KB). The `bram_spram_32bits` unit defines an address space with
BRAM located after SPRAM.
Since the SPRAM cannot be initialized at startup, a small boot code resides in BRAM
and copies the code to run from *flash memory* (more on that later) to the larger
SPRAM, then the CPU jumps to this code.
This is done in [boot.c](../software/boot/boot.c):
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../software/boot/boot.c&syntax=c&lines=32-39) -->
<!-- The below code snippet is automatically added from ../software/boot/boot.c -->
```c
  // copy to the start of the memory segment
  unsigned char *code = (unsigned char *)0x0000004;
  spiflash_copy(PAYLOAD_OFFSET/*offset*/,code,131088/*SPRAM size*/);
  //                                          ^^^^^^ max code image size
  // jump!
  *LEDS = 15;
  synch =  1; // sync with other core
  asm volatile ("li t0,4; jalr x0,0(t0);");
```
<!-- MARKDOWN-AUTO-DOCS:END -->

This boot code is compiled and stored in the initialized BRAM, that's how
our CPU is able to start, using for boot address the start of the BRAM
in `bram_spram_32bits`.

Now, what's this *flash memory* we mentioned earlier? In most cases it is the
on board SPIflash chip that stores FPGA configuration. This is a relatively large
memory (some MB) that is very slow to write to, but can be relatively fast to
read from (using quad SPI at 100 MHz one can reach 50 MB/sec, in ideal contiguous
burst reads). The DMC-1 design uses this memory as our texture memory, but we also
store the CPU code to be loaded at boot in there. In practice, the code is stored
at offset 1MB and the textures at offset 2MB in the flash memory.

> On the MCH2022 badge there is no flash memory but instead a volatile PSRAM chip.
> The protocol is nearly identical (quad SPI), but the PSRAM has to be pre-loaded
> with code and texture data *before* the design is started. This is achieved
> using another design to load in PSRAM from UART, then reconfiguring the FPGA
> with our hardware. (As long as power is maintained the PSRAM retains its content).

###  Do not cross the streams

Of course, since both the CPU and GPU may access flash memory, we have to be a bit
careful. In our design however, things are quite simple: when the CPU boots,
the GPU does nothing and thus is not accessing the memory. We can then have a
very simple mechanism:

First, note that the CPU is bound to a `texmem_io` group called `texio`:

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

These variables are updated in the always block:
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=344-348) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    // track texture memory interface
    txm.in_ready            = txm_io.in_ready;
    txm.addr                = txm_io.addr;
    txm_io.data             = txm.rdata;
    txm_io.busy             = txm.busy;
```
<!-- MARKDOWN-AUTO-DOCS:END -->
As you can see, this connects `texio` to `txm` which is the flash memory unit itself:
`qpsram_ram txm...` (on mch2022) or `spiflash_rom_core` (on icebreaker).

To access flash the CPU first uses memory mapping to set an address and start a
read:
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=404-408) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
            case 2b10: { // texture memory CPU write
              txm.addr     = prev_mem_wdata[0,24];
              txm.in_ready = 1; // NOTE: we assume there cannot be collisions
                               // with the texture sampler ; CPU should not
                               // drive the texture while draw calls are pending
```
<!-- MARKDOWN-AUTO-DOCS:END -->
The read byte and the bit indicating the memory controller is busy are given to the
CPU through its `user_data` registry (32 bits), that the RISC-V code can use to wait for
the read to complete and get the value:
```c
user_data[0,24] = { ..., txm.rdata, ... , txm.busy, ... };
```
(`txm.rdata` is 8 bits as the texture memory interface reads bytes.)

### GPU, command queue and screen

The rest of the SOC is dedicated to the command queue and screen. Let's first
take a look at the command queue. Whenever the CPU wants to draw, it sends a
command, or *draw call* to the GPU.
This is done through memory mapping,
intercepting memory addresses that are outside the range of the actual available
memory. Because each command is 64 bits, this takes two memory writes:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=413-423) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
        case 4b0001: { // GPU commands
          if (prev_mem_addr[0,1]) {
            // received first half
            cmdq.in_command[32,32] = prev_mem_wdata[0,32];
          } else {
            // received second half
            cmdq.in_command[ 0,32] = prev_mem_wdata[0,32];
            // store in fifo
            cmdq.in_add = 1;
          }
        }
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The `case 4b0001:` is part of a `switch` performed on accessed memory addresses,
when a high bit of the address is set (well outside the truly available
memory space):

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=371-372) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    if ((prev_mem_wenable != 0) & prev_mem_addr[16,1]) {
      switch (prev_mem_addr[2,4]) {
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The command queue is a FIFO data structure that records the commands.
Each command is added to the FIFO by pulsing `cmdq.in_add`.
The GPU then attempts to process them as fast as possible. Of course the command queue
might fill up, a situation the CPU has to know about. This again uses the `user_data`
register:
```c
user_data[0,24] = { ... , ~cmdq.full, ... };
```
The `cmdq.full` signal will become high sometime before the FIFO is catastrophically
full, so that the CPU can still send a few commands. But then it should stop, or
the FIFO will overflow and bad things happen (typically, the GPU hangs).

We send the next command to the GPU with these two lines:
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=428-430) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    // send next command to GPU
    gpu.valid    = gpu.ready & ~cmdq.empty;
    cmdq.in_next = gpu.ready & ~cmdq.empty;
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The first line pulses `gpu.valid` to tell the GPU to process the command, while
`cmdq.in_next` tells the command queue to move to the next command. The command
itself is given to the GPU through a binding:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=320-322) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // command queue
  command_queue cmdq(current :> gpu.command);
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The GPU is driving the screen directly, with the CPU only having access during
initialization (a similar situation than with the flash memory). This can be
seen in particular in this part of the SOC:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=356-359) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    // screen interface
    screen_ctrl   .valid    = gpu.screen_valid;
    screen_ctrl   .in_data  = gpu.screen_valid
                            ? {1b1,gpu.screen_data} : prev_mem_wdata[0,17];
```
<!-- MARKDOWN-AUTO-DOCS:END -->

Note that `screen_ctrl.in_data` receives data either from the GPU
or the CPU. The CPU can trigger `screen_ctrl.valid` through a memory mapped address:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=394-396) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
            case 2b01: { // screen write from CPU
              screen_ctrl.valid = 1;
            }
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The screen controller unit `screen_ctrl` then drives a lower level screen driver
`screen` that talks to the actual screen though either parallel (e.g. mch2022 badge)
or SPI protocols (e.g. icebreaker).

Alright, enough of the SOC, let's dissect the GPU!

> There are some other details about the SOC, in particular to setup for simulation
> and to introduce latencies to relax the pressure on the maximum frequency. All
> the lines starting with `$$` are pre-processor lines selecting between various
> options and configuring some parameters for the CPU and other units.

## Part II - The GPU

> To be written