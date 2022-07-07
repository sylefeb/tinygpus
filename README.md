# TinyGPUs
*Making graphics hardware like its 1990, explained*

## Objectives

The tinyGPUs project started with the following question: *"What would have resembled graphics hardware dedicated to our beloved retro-games from the early 90's, such as Doom 1993 and Comanche 1992?"*. This led me to creating the `dmc-1` GPU, the first (and currently only!) tiny GPU in this repository.

> **Note:** `dmc` stands for *Doom Meets Comanche*... also, it sounds cool (any resemblance to a car name is of course pure coincidence).

However, the true objective is to *explore and explain* basic graphics hardware design. Don't expect to learn anything about modern GPUs, but rather expect to learn about fundamental graphics algorithms, their elegant simplicity, and how to turn these algorithms into hardware on FPGAs. And that is *surprisingly simple*.

> **Note:** The `dmc-1` is powering my *Doom-chip "onice"* demo, about which I gave a talk at rC3 nowhere in December 2021. You can [watch it here](https://youtu.be/2ZAIIDXoBis) and [browse the slides here](https://www.antexel.com/doomchip_onice_rc3/).

> **Note:** There is a plan to do another tiny GPU, hence the **s** in tinyGPUs, exploring different design tradeoffs. But that will come later.

The tinyGPUs are written in [Silice](https://github.com/sylefeb/Silice), with bits and pieces of Verilog.

## Running the demos

The demos are running both on the `icebreaker` board and the `MCH2022` badge.

### In simulation

For the rotating tetrahedron demo:
```
cd demos
make simulation DEMO=tetrahedron
```

For the terrain demo:
```
cd demos
make simulation DEMO=terrain
```

For the doomchip-onice demo:
```
cd demos
make simulation DEMO=doomchip-onice
```

### On the icebreaker

A [240x320 SPIscreen with a ST7789 driver](https://www.waveshare.com/2inch-lcd-module.htm) has to be hooked to the PMOD 1A, following this pinout:

| PMOD1A       | SPIscreen  |
|--------------|------------|
| pin 1        | rst        |
| pin 2        | dc         |
| pin 3        | clk        |
| pin 4        | din        |
| pin 5  (GND) | screen GND |
| pin 6  (VCC) | screen VCC |
| pin 11 (GND) | cs         |
| pin 12 (VCC) | bl         |

For the rotating tetrahedron demo:
```
cd demos
make BOARD=icebreaker DEMO=tetrahedron program_all
```

For the terrain demo:
```
cd demos
make BOARD=icebreaker DEMO=terrain program_all
```

> **Note:** `program_all` takes a long time as it transfers the texture data
onto the board. After doing it once, to test other demos replace `program_all`
by `program_code`.

### On the MCH2022 badge

To be written ...

```
make BOARD=mch2022 DEMO=doomchip-onice program_all MCH2022_PORT=/dev/ttyACM1
```

## Credits
- Data files for terrains are downloaded from *s-macke*
[VoxelSpace repository](https://github.com/s-macke/VoxelSpace/) which also
contains excellent explanations regarding the Comanche terrain rendering algorithm.
- Doom shareware data, `doom1.wad`, is automatically downloaded and is of course
part of the original game by Id Software.
