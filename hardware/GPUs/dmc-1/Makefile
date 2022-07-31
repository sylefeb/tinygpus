SILICE_DIR=$(dir $(shell which silice))

all: ice40 icarus clean

ice40: dmc-1-standalone.si
	mkdir -p export
	mkdir -p BUILD_export
	make -C ../../../demos/data ../build/palette666.si
	pushd . ; cd ../../common ; ./download_all.sh ; popd
	silice dmc-1-standalone.si --output BUILD_export/dmc-1-gpu.v \
	--export DMC_1_gpu_standalone \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-D ICE40=1 \
	-D MCH2022=1
	cp BUILD_export/dmc-1-gpu.v export/

icarus: dmc-1-standalone.si
	mkdir -p export
	mkdir -p BUILD_export
	make -C ../../../demos/data ../build/palette666.si
	pushd . ; cd ../../common ; ./download_all.sh ; popd
	silice dmc-1-standalone.si --output BUILD_export/dmc-1-gpu-icarus.v \
	--export DMC_1_gpu_standalone \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-D ICARUS=1 \
	-D SIMULATION=1 \
	-D SIMUL_QPSRAM=1 \
	-C reg_init_zero=1
	cp BUILD_export/dmc-1-gpu-icarus.v export/

clean:
	rm -rf BUILD_export