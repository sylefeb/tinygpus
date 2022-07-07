#!/bin/bash
set -e
export DIR=`pwd`
echo $DIR
export RV32IM=1
pushd .
./compile.sh $1 --nolibc
cd build
riscv64-unknown-elf-objcopy.exe -O binary code.elf code.bin
riscv64-unknown-elf-objdump.exe -D -b binary -m riscv code.bin > disasm.txt
cd ..
make spiflash
# cp data.img code.img
# iceprog -o 1M code.img
