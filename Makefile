all: ecmtool ecmtool.exe

ecmtool:
	# Copy the makefile
	cp makefile_zlib/Makefile.linux zlib/Makefile.linux
	# First cleanup to be sure
	make -C zlib -f Makefile.linux clean
	# Compile the zlib static library for linux
	make -C zlib -f Makefile.linux

	# Compile the Linux release
	mkdir -p release/linux
	g++ -o release/linux/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -Lzlib -lzlinux -Izlib

	# Clean the zlib directory at end
	make -C zlib -f Makefile.linux clean
	rm zlib/Makefile.linux

ecmtool.exe:
	# Copy the makefile
	cp makefile_zlib/Makefile.win zlib/Makefile.win
	# First cleanup to be sure
	make -C zlib -f Makefile.win clean
	# Compile the zlib static library for Windows
	make -C zlib -f Makefile.win

	# Compile the Win64 release
	mkdir -p release/win64
	x86_64-w64-mingw32-g++ -O2 -ffunction-sections -Wl,-gc-sections -static-libstdc++ -s -o release/win64/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -Izlib zlib/libzwindows.a

	# Clean the zlib directory at end
	make -C zlib -f Makefile.win clean
	rm zlib/Makefile.win

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 ecmtool $(DESTDIR)/usr/bin/

.PHONY: clean

clean:
	rm ecmtool.o
	rm -Rf release
	# Clean the zlib directory
	cp makefile_zlib/Makefile.linux zlib/Makefile.linux
	make -C zlib -f Makefile.linux clean
	rm zlib/Makefile.linux
	cp makefile_zlib/Makefile.win zlib/Makefile.win
	make -C zlib -f Makefile.win clean
	rm zlib/Makefile.win