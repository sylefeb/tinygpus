all: qrepack/qrepack
	./qrepack/qrepack

qrepack/qrepack:
	git submodule init
	git submodule update
	wget -nc https://github.com/fzwoch/quake_map_source/raw/master/bsp/e1m1.bsp
	cd qrepack ; cmake . -G "Unix Makefiles" ; make

clean:
	rm -f qrepack/qrepack
	rm -f qrepack/qrepack.exe
	rm -f ../build/quake.img
