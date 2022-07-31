SILICE_DIR=$(dir $(shell which silice))

ice40: dmc-1-standalone.si
	mkdir -p export
	make -C ../../../demos/data ../build/palette666.si
	pushd . ; cd ../../common ; ./download_all.sh ; popd
	silice dmc-1-standalone.si --output export/dmc-1-gpu.v \
	--export DMC_1_gpu_standalone \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-D ICE40=1 \
	-D MCH2022=1

icarus: dmc-1-standalone.si
	mkdir -p export
	make -C ../../../demos/data ../build/palette666.si
	pushd . ; cd ../../common ; ./download_all.sh ; popd
	silice dmc-1-standalone.si --output export/dmc-1-gpu-icarus.v \
	--export DMC_1_gpu_standalone \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-D ICARUS=1 \
	-C reg_init_zero=1

clean:
	rm -rf export