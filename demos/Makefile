DEMO  ?= tetrahedron
BOARD ?= icebreaker
MCH2022_PORT ?= /dev/ttyACM1

SOC_PATH=../hardware/SOCs/ice40-dmc-1/
SW_PATH =../software/compile/ice40/

$(BOARD) : build/$(BOARD).bin build/$(DEMO).img

simulation : build/$(DEMO).img build/textures.img $(SOC_PATH)/soc-ice40-dmc-1-risc_v.si
	mkdir -p build
	head -c 1M < /dev/urandom > rnd.raw
	cat rnd.raw build/$(DEMO).img > data_tmp.raw
	truncate -s 2M data_tmp.raw
	cat data_tmp.raw build/textures.img > $(SOC_PATH)/data.raw
	rm rnd.raw data_tmp.raw
	make -C $(SOC_PATH) -f Makefile verilator

build/$(BOARD).bin : $(SOC_PATH)/soc-ice40-dmc-1-risc_v.si
	mkdir -p build
	make -C $(SOC_PATH) -f Makefile $(BOARD)
	cp $(SOC_PATH)/BUILD_$(BOARD)/build.bin build/$(BOARD).bin

build/$(DEMO).img : $(DEMO)/$(DEMO).c build/level.h
	mkdir -p build
	$(SW_PATH)/compile_c.sh $(DEMO)/$(DEMO).c
	make -C $(SW_PATH)
	cp $(SW_PATH)/build/code.img build/$(DEMO).img

build/textures.img:
	cd data ; ./get_data.sh
	make -C data ../build/textures.img

build/level.h:
	cd data ; ./get_data.sh
	make -C data ../build/level.h

program_all: build/$(BOARD).bin build/$(DEMO).img build/textures.img
ifeq ($(BOARD),icebreaker)
	iceprog -o 2M build/textures.img
	iceprog -o 1M build/$(DEMO).img
	iceprog build/$(BOARD).bin
else
ifeq ($(BOARD),mch2022)
	./build/webusb_fpga.py build/write.bin
	sleep 0.5
	python ./build/send.py $(MCH2022_PORT) 1048576 build/$(DEMO).img
	./build/webusb_fpga.py build/write.bin
	sleep 0.5
	python ./build/send.py $(MCH2022_PORT) 2097152 build/textures.img
	./build/webusb_fpga.py build/$(BOARD).bin
else
	echo "no yet implemented"
endif
endif

program_design: build/$(BOARD).bin
ifeq ($(BOARD),icebreaker)
	iceprog build/$(BOARD).bin
else
ifeq ($(BOARD),mch2022)
	./build/webusb_fpga.py build/$(BOARD).bin
else
	echo "no yet implemented"
endif
endif

program_code: build/$(DEMO).img
ifeq ($(BOARD),icebreaker)
	iceprog -o 1M build/$(DEMO).img
else
ifeq ($(BOARD),mch2022)
	./build/webusb_fpga.py build/write.bin
	sleep 0.5
	python ./build/send.py $(MCH2022_PORT) 1048576 build/$(DEMO).img
	./build/webusb_fpga.py build/$(BOARD).bin
else
	echo "no yet implemented"
endif
endif

mch2022prog:
	mkdir -p build
	-wget -nc https://raw.githubusercontent.com/badgeteam/mch2022-tools/master/webusb_fpga.py -O build/webusb_fpga.py
	chmod 755 build/webusb_fpga.py
	-wget -nc https://github.com/sylefeb/mch2022-silice/raw/main/qpsram_loader/bitstreams/write.bin -O build/write.bin
	-wget -nc https://github.com/sylefeb/mch2022-silice/raw/main/qpsram_loader/send.py -O build/send.py

ifeq ($(BOARD),mch2022)
program_all:    mch2022prog
program_code:   mch2022prog
program_design: mch2022prog
endif

clean :
	rm -rf build
	make -C data clean
	make -C $(SOC_PATH) -f Makefile clean
	make -C $(SW_PATH) clean
