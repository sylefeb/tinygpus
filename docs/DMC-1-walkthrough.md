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

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=208-213) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // CPU
  uint32 user_data(0);
  bram_port_io mem_io_cpu;
  rv32i_cpu cpu(
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The CPU ([my ice-v-dual CPU](https://github.com/sylefeb/Silice/blob/master/projects/ice-v/README.md))

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=215-221) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    mem              <:> mem_io_cpu,
  );

  // ==============================
  // GPU
  texmem_io     txm_io;
  DMC_1_gpu gpu(txm          <:>  txm_io,
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=224-228) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // screen display
  uint1 screen_valid(0);    uint1 screen_ready(0);
  uint1 screen_send_dc(0);  uint8 screen_send_byte(0);
  screen_controller screen_ctrl(
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU texture memory

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=274-280) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
      ram_io2 <:> ram_io2,  ram_io3 <:> ram_io3,
    );
$else -- not MCH2022
    spiflash_rom_core txm<@clock2x,!rst2x,reginputs> (
      sf_clk  :> sf_clk,    sf_csn  :> sf_csn,
      sf_io0 <:> sf_io0,    sf_io1 <:> sf_io1,
      sf_io2 <:> sf_io2,    sf_io3 <:> sf_io3,
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU command queue

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=336-338) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  always {
```
<!-- MARKDOWN-AUTO-DOCS:END -->

- The screen controller and screen driver

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=230-240) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    screen_ready <: screen_ready,
    send_dc      :> screen_send_dc,
    send_byte    :> screen_send_byte,
  );
  // ...

$if MCH2022 then
  screen_driver screen(
    ready            :> screen_ready,
    screen_d         :> lcd_d,
    screen_dc        :> lcd_rs,
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
  volatile int *code = (volatile int *)0x0000004;
  spiflash_copy(PAYLOAD_OFFSET/*offset*/,code,131064/*SPRAM size*/);
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
careful. The SOC implements a mechanism so that the CPU can request memory
transfers from SPIflash to its RAM, but the CPU has to ensure there is no
ongoing GPU activity before using this. This is true at boot time or when the
GPU reports an empty command queue, as indicated by bit 2 of `user_data`.

First, note that the GPU is bound to a `texmem_io` group called `txm_io`:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=224-228) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  // ==============================
  // screen display
  uint1 screen_valid(0);    uint1 screen_ready(0);
  uint1 screen_send_dc(0);  uint8 screen_send_byte(0);
  screen_controller screen_ctrl(
```
<!-- MARKDOWN-AUTO-DOCS:END -->

These variables are updated in the always block:
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=366-371) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    txm_io.busy             = txm.busy;
$if MCH2022 then
    txm.wenable             = 0; // we never write to PSRAM
$end
$if SIMULATION then
    //if ((|mem_io_ram.wenable) & txm_burst_data_available) {
```
<!-- MARKDOWN-AUTO-DOCS:END -->
As you can see, this connects `txm_io` to `txm` which is the flash memory unit itself:
`qpsram_ram txm...` (on mch2022) or `spiflash_rom_core` (on icebreaker).

To access flash the CPU first uses memory mapping to set an address and initiate a
transfer:
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=449-472) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
              //        and should wait on user_data, txm.busy bit
              // NOTE3: destination addresses have to be 32-bits aligned
              uint1 len_else_dst    = prev_mem_wdata[31,1];
              // ^^^ bit 31 indicates whether we are setting the length (bytes)
              // or the destination address (in ram)
              uint1 src_and_trigger = prev_mem_wdata[30,1];
              // ^^^ bit 31 indicates whether we are setting the source (in tex
              // memory) or the destination address (in ram)
              // Set size, then destination, then source (triggers the burst)
              if (len_else_dst) {
                txm_burst_len       = prev_mem_wdata[2,22];
              } else {
                if (~src_and_trigger) {
                  txm_burst_read_dst  = prev_mem_wdata[2,22];
                }
              }
              txm.addr              = prev_mem_wdata[0,24];
              txm_burst_read_active = src_and_trigger; // start
              txm_burst_data_cursor = 5b10000;
$if SIMULATION then
              //if (src_and_trigger) {
              //  __display("[cycle %d] BURST start, from @%x to @%x, len %d",iter,txm.addr,txm_burst_read_dst<<2,txm_burst_len);
              //}
$end
```
<!-- MARKDOWN-AUTO-DOCS:END -->

This immediately initiates a transfer. During a transfer the CPU is stalled: its
RAM is being written every cycle and it can no longer fetch instruction.
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=380-381) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
                                                : txm_burst_data;
      txm_burst_data_cursor =
```
<!-- MARKDOWN-AUTO-DOCS:END -->

### GPU, command queue and screen

The rest of the SOC is dedicated to the command queue and screen. Let's first
take a look at the command queue. Whenever the CPU wants to draw, it sends a
command, or *draw call* to the GPU.
This is done through memory mapping,
intercepting memory addresses that are outside the range of the actual available
memory. Because each command is 64 bits, this takes two memory writes:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=482-492) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
            // received second half
            cmdq.in_command[ 0,32] = prev_mem_wdata[0,32];
            // store in fifo
            cmdq.in_add = 1;
          }
        }
        case 4b1000: { // GPU commands in part
          switch (prev_mem_addr[6,4]) {
            // tex0 PARAMETER_PLANE_A     (2<<30) | ((ny) & 1023)  | (((uy) & 1023)<<10) | (((vy) & 1023)<<20)
            case 0: { // PARAMETER_PLANE_A_ny
              cmdq.in_command[$32+ 0$,10] = prev_mem_wdata[0,10];
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The `case 4b0001:` is part of a `switch` performed on accessed memory addresses,
when a high bit of the address is set (well outside the truly available
memory space):

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=415-417) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
          uo.data_in       = prev_mem_wdata[ 0,8];
          uo.data_in_ready = ~uo.busy;
$if SIMULATION then
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
<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=561-563) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    prev_mem_addr    = mem_io_cpu.addr;
    prev_mem_wenable = mem_io_cpu.wenable;
    prev_mem_wdata   = mem_io_cpu.wdata;
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The first line pulses `gpu.valid` to tell the GPU to process the command, while
`cmdq.in_next` tells the command queue to move to the next command. The command
itself is given to the GPU through a binding:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=336-337) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
  always {
```
<!-- MARKDOWN-AUTO-DOCS:END -->

The GPU is driving the screen directly, with the CPU only having access during
initialization. This can be seen in particular in this part of the SOC:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=401-404) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
    screen.data_or_command  = screen_send_dc;
    screen.byte             = screen_send_byte;
    // uart: maintain low, pulse on send
    uo.data_in_ready = 0;
```
<!-- MARKDOWN-AUTO-DOCS:END -->

Note that `screen_ctrl.in_data` receives data either from the GPU
or the CPU. The CPU can trigger `screen_ctrl.valid` through a memory mapped address:

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si&syntax=c&lines=439-441) -->
<!-- The below code snippet is automatically added from ../hardware/SOCs/ice40-dmc-1/soc-ice40-dmc-1-risc_v.si -->
```c
              oled_resn = ~ prev_mem_wdata[0,1];
$elseif not MCH2022 then
              o_resn    = ~ prev_mem_wdata[0,1];
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