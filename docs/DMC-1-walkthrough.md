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
<!-- MARKDOWN-AUTO-DOCS:END -->

- The CPU ([my ice-v-dual CPU](https://github.com/sylefeb/Silice/blob/master/projects/ice-v/README.md))

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=213-219) -->
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU (!)

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=221-225) -->
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU texture memory

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=271-278) -->
<!-- MARKDOWN-AUTO-DOCS:END -->

- The GPU command queue

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=320-322) -->
<!-- MARKDOWN-AUTO-DOCS:END -->

- The screen controller and screen driver

<!-- MARKDOWN-AUTO-DOCS:START (CODE:src=../hardware/GPUs/dmc-1/dmc-1.si&syntax=c&lines=227-237) -->
<!-- MARKDOWN-AUTO-DOCS:END -->
