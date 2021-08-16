COMP_OPT=-O2 -ffunction-sections -Wl,-gc-sections -s -Izlib -Ixz/src/liblzma/api/ -Lxz/src/liblzma/.libs/ -Lzlib -Ilz4/lib/ -Ilzlib4 -Iflaczlib -Iflac/include

all: ecmtool ecmtool.exe

ecmtool:
	########## ZLIB MAKE ##########
	# Copy the makefile
	cp makefile_zlib/Makefile.linux zlib/Makefile.linux
	# First cleanup to be sure
	make -C zlib -f Makefile.linux clean
	# Compile the zlib static library for linux
	make -C zlib -f Makefile.linux -j$(nproc)
	########## END ZLIB MAKE ##########

	########## LZMA (XZ) MAKE ##########
	cd xz && bash autogen.sh --no-po4a && ./configure --enable-static --disable-xz --disable-xzdec --disable-lzmainfo --disable-lzma-links --disable-scripts --disable-doc --disable-lzmadec --disable-shared 
	make -C xz -j$(nproc)
	########## END LZMA (XZ) MAKE ##########

	########## FLAC MAKE ##########
	cd flac && bash autogen.sh && ./configure --enable-static --disable-xmms-plugin --disable-ogg --disable-oggtest --disable-examples --enable-shared=no --disable-cpplibs --disable-doxygen-docs
	make -C flac clean
	make -C flac -j$(nproc)
	########## END FLAC MAKE ##########

	# Compile the Linux release
	mkdir -p release/linux
	g++ ${COMP_OPT} -o release/linux/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -lzlinux -llzma lz4/lib/lz4hc.c lz4/lib/lz4.c lzlib4/lzlib4.cpp flaczlib/flaczlib.cpp flac/src/libFLAC/.libs/libFLAC-static.a

	########## ZLIB CLEAN ##########
	# Clean the zlib directory at end
	make -C zlib -f Makefile.linux clean
	rm zlib/Makefile.linux
	########## END ZLIB CLEAN ##########

	########## LZMA CLEAN ##########
	make -C xz clean
	########## END LZMA CLEAN ##########

	########## FLAC CLEAN ##########
	make -C flac clean
	########## END FLAC CLEAN ##########

	

ecmtool.exe:
    ########## ZLIB MAKE ##########
	# Copy the makefile
	cp makefile_zlib/Makefile.win zlib/Makefile.win
	# First cleanup to be sure
	make -C zlib -f Makefile.win clean
	# Compile the zlib static library for Windows
	make -C zlib -f Makefile.win -j$(nproc)
	########## END ZLIB MAKE ##########

	########## LZMA (XZ) MAKE ##########
	cd xz && bash autogen.sh --no-po4a && CC=x86_64-w64-mingw32-gcc ./configure --host=x64-windows --target=x64-windows --enable-static --disable-xz --disable-xzdec --disable-lzmainfo --disable-lzma-links --disable-doc --disable-scripts --disable-lzmadec --disable-shared --enable-threads=vista
	make -C xz -j$(nproc)
	########## LZMA (XZ) MAKE ##########

	########## FLAC MAKE ##########
	cd flac && bash autogen.sh && ./configure --host=x86_64-w64-mingw32 --enable-static --disable-xmms-plugin --disable-ogg --disable-oggtest --disable-examples --enable-shared=no --disable-cpplibs --disable-doxygen-docs
	make -C flac clean
	cd flac && make -j$(nproc)
	########## END FLAC MAKE ##########

	# Compile the Win64 release
	mkdir -p release/win64
	x86_64-w64-mingw32-g++ ${COMP_OPT} -o release/win64/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -lzwindows -llzma lz4/lib/lz4hc.c lz4/lib/lz4.c lzlib4/lzlib4.cpp flaczlib/flaczlib.cpp flac/src/libFLAC/.libs/libFLAC-static.a

	########## ZLIB CLEAN ##########
	# Clean the zlib directory at end
	make -C zlib -f Makefile.win clean
	rm zlib/Makefile.win
	########## END ZLIB CLEAN ##########

	########## LZMA CLEAN ##########
	make -C xz clean
	########## END LZMA CLEAN ##########

	########## FLAC CLEAN ##########
	make -C flac clean
	########## END FLAC CLEAN ##########

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 ecmtool $(DESTDIR)/usr/bin/

.PHONY: clean

clean:
	rm -f ecmtool.o
	rm -Rf release
	# Clean the zlib directory
	cp makefile_zlib/Makefile.linux zlib/Makefile.linux
	make -C zlib -f Makefile.linux clean
	rm -f zlib/Makefile.linux
	cp makefile_zlib/Makefile.win zlib/Makefile.win
	make -C zlib -f Makefile.win clean
	rm -f zlib/Makefile.win
