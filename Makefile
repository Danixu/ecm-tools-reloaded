OBJ = ecmtool.o sector_tools.o

all: ecmtool ecmtool.exe

ecmtool: $(OBJ)
	mkdir -p release/linux
	g++ $(CFLAGS) -o release/linux/$@ ecmtool.cpp sector_tools.o

ecmtool.exe: $(OBJ)
	mkdir -p release/win64
	x86_64-w64-mingw32-g++ $(CFLAGS) -O2 -ffunction-sections -Wl,-gc-sections -static-libstdc++ -s -o release/win64/$@ ecmtool.cpp sector_tools.cpp

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 ecmtool $(DESTDIR)/usr/bin/

.PHONY: clean

clean:
	rm ecmtool.o
	rm -Rf release
