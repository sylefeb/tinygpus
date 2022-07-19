SILICE_DIR=$(dir $(shell which silice))

dmc-1.v: dmc-1-standalone.si
	mkdir -p export
	make -C ../../../demos/data ../build/palette666.si
	silice dmc-1-standalone.si --output export/dmc-1-gpu.v \
	--export DMC_1_gpu_standalone \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-D ICE40=1 \
	-D MCH2022=1

clean:
	rm -rf export