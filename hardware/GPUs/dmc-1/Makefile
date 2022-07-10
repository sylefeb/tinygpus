SILICE_DIR=$(dir $(shell which silice))

dmc-1.v: dmc-1.si
	mkdir -p export
	silice dmc-1.si --output export/dmc-1-gpu.v \
	--export DMC_1_gpu \
	--frameworks_dir $(SILICE_DIR)/../frameworks/ \
	--framework $(SILICE_DIR)/../frameworks/boards/bare/bare.v \
	-P SF_IN_READY_INIT=0   \
	-P SF_IN_READY_WIDTH=1  \
	-P SF_IN_READY_SIGNED=0 \
	-P SF_ADDR_INIT=0 \
	-P SF_ADDR_WIDTH=24 \
	-P SF_ADDR_SIGNED=0 \
	-P SF_DATA_INIT=0 \
	-P SF_DATA_WIDTH=8 \
	-P SF_DATA_SIGNED=0 \
	-P SF_BUSY_INIT=0   \
	-P SF_BUSY_WIDTH=1  \
	-P SF_BUSY_SIGNED=0

clean:
	rm -rf export