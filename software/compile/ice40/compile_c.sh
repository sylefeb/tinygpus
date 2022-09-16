#!/bin/bash
CALL_DIR=`pwd`
SCRI_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

pushd .
cd $SCRI_DIR

API=$SCRI_DIR/../../api/

wget -nc https://raw.githubusercontent.com/sylefeb/Silice/draft/tools/bash/find_riscv.sh
source ./find_riscv.sh

riscarch="rv32im"

echo "using $ARCH"
echo "targetting $riscarch"

rm -rf build
mkdir -p build

OBJECTS="build/code.o"

if [[ -z "${CONFIG_LD}" ]]; then
  #CFG_LD="config_c.ld"
  CFG_LD="config_c_swirl.ld"
else
  CFG_LD="${CONFIG_LD}"
fi

$ARCH-as -march=$riscarch -mabi=ilp32 -o crt0.o crt0.s

$ARCH-gcc -g -O3 -nostartfiles -ffunction-sections -fdata-sections -fno-stack-protector -fno-pic -fno-builtin -march=$riscarch -mabi=ilp32 -I$CALL_DIR -I$API -I$SCRI_DIR -T $CFG_LD -o build/code.elf $CALL_DIR/$1 crt0.o

$ARCH-objcopy -O verilog build/code.elf build/code.hex

# uncomment to dump a disassembly of the code, usefull for debugging
$ARCH-objdump.exe -drwCS build/code.elf > build/disasm.txt

popd
