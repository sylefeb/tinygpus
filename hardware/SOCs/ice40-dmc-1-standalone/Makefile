icebreaker: prepare soc-ice40-dmc-1-standalone.si
	silice-make.py --no_program -s soc-ice40-dmc-1-standalone.si -b $@ -p basic,qspiflash,oled,buttons,uart -o BUILD_$(subst :,_,$@) -t shell

mch2022: prepare soc-ice40-dmc-1-standalone.si
	silice-make.py --no_program -s soc-ice40-dmc-1-standalone.si -b $@ -p basic,psram,lcd,uart -o BUILD_$(subst :,_,$@)

verilator: prepare soc-ice40-dmc-1-standalone.si
	silice-make.py -s soc-ice40-dmc-1-standalone.si -b $@ -p basic,oled -o BUILD_$(subst :,_,$@) -t shell

prepare:
	rm -rf ./build
	pushd . ; cd ../../common ; ./download_all.sh ; popd
	pushd . ; cd ../../../software/compile/ice40/ ; ./compile_boot.sh ; make ; popd
	cp -Rf ../../../software/compile/ice40/build .
	make -C ../../../demos/data ../build/palette666.si

clean:
	rm -rf BUILD_*
	rm -rf ./build
