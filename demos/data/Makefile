../build/textures.img: pre.si
	mkdir -p ../build
	./get_data.sh
	silice-make.py -s pre.si -b bare -o BUILD_textures

../build/palette666.si: pre.si
	mkdir -p ../build
	./get_data.sh
	silice-make.py -s pre.si -b bare -o BUILD_textures

../build/level.h: pre.si
	mkdir -p ../build
	./get_data.sh
	silice-make.py -s pre.si -b bare -o BUILD_textures

clean:
	rm -f ../build/textures.img
	rm -f ../build/palette666.si
	rm -f ../build/level.h
	rm -rf ./BUILD_*/
	rm -f *.tga
	rm -f *.png