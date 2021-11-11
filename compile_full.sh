x86_64-w64-mingw32-g++ -O3 -Wl,-gc-sections -Izlib -Ixz/src/liblzma/api/ -Lxz/src/liblzma/.libs/ -Lzlib -Ilz4/lib/ -Ilzlib4 -Iflaczlib -Iflac/include -o /mnt/c/Users/danix/Desktop/ecmtool.exe ecmtool.cpp compressor.cpp sector_tools.cpp -lzwindows -llzma lz4/lib/lz4hc.c lz4/lib/lz4.c lzlib4/lzlib4.cpp flaczlib/flaczlib.cpp flac/src/libFLAC/.libs/libFLAC-static.a

## Debug options
# -g -pg
