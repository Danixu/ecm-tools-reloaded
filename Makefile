zlib_version = 1.2.11

all: ecmtool ecmtool.exe

ecmtool:
	# First cleanup to be sure
	make -C zlib-${zlib_version} -f Makefile.linux clean
	# Compile the zlib static library for linux
	make -C zlib-${zlib_version} -f Makefile.linux

	# Compile the Linux release
	mkdir -p release/linux
	g++ -o release/linux/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -Lzlib-${zlib_version} -lzlinux -Izlib-${zlib_version}

	# Clean the zlib directory at end
	make -C zlib-${zlib_version} -f Makefile.linux clean

ecmtool.exe:
	# First cleanup to be sure
	make -C zlib-${zlib_version} -f Makefile.win clean
	# Compile the zlib static library for Windows
	make -C zlib-${zlib_version} -f Makefile.win

	# Compile the Win64 release
	mkdir -p release/win64
	x86_64-w64-mingw32-g++ -O2 -ffunction-sections -Wl,-gc-sections -static-libstdc++ -s -o release/win64/$@ ecmtool.cpp compressor.cpp sector_tools.cpp -Izlib-${zlib_version} zlib-${zlib_version}/libzwindows.a

	# Clean the zlib directory at end
	make -C zlib-${zlib_version} -f Makefile.win clean

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 ecmtool $(DESTDIR)/usr/bin/

.PHONY: clean

clean:
	rm ecmtool.o
	rm -Rf release
	# Clean the zlib directory
	make -C zlib-${zlib_version} -f Makefile.linux clean
	make -C zlib-${zlib_version} -f Makefile.win clean