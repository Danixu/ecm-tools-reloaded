CCW = x86_64-w64-mingw32-gcc

DEPS = banner.h common.h version.h

OBJ = ecm.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: bin2ecm ecm2bin bin2ecm.exe ecm2bin.exe

bin2ecm: $(OBJ)
	echo %.c
	mkdir -p release/linux
	$(CC) $(CFLAGS) -o release/linux/$@ $<

ecm2bin: $(OBJ)
	mkdir -p release/linux
	$(CC) $(CFLAGS) -o release/linux/$@ $<

bin2ecm.exe: $(OBJ)
	mkdir -p release/win64
	$(CCW) $(CFLAGS) -O2 -ffunction-sections -Wl,-gc-sections -static-libstdc++ -s -o release/win64/$@ ecm.c

ecm2bin.exe: $(OBJ)
	mkdir -p release/win64
	$(CCW) $(CFLAGS) -O2 -ffunction-sections -Wl,-gc-sections -static-libstdc++ -s -o release/win64/$@ ecm.c

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 bin2ecm $(DESTDIR)/usr/bin/
	ln -s bin2ecm $(DESTDIR)/usr/bin/ecm2bin

.PHONY: clean

clean:
	rm ecm.o
	rm -Rf release
