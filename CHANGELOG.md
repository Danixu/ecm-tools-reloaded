# Changelog

### v2.3.1-alpha

* Now if a CD-ROM image contains any wrong sector MSF (like PSX libcript protection), then MSF cleanup is disabled because the process will not be lossles if not.
* Fixed a bug in lzma compression which makes the program to create bigger files. Was much more notizable in images with a lot of merged CDDA and data sectors (Medievil ECM file was 2gb+, which is much more than the original image).
* If there was an error processing the file, then the output file will be removed (added an option to keep it).

### v2.3.0-alpha

* Added FLAC compression using flaczlib class which helps to make it in a similar way to zlib
* Added FLAC repository as submodule and updated Makefile
* Fixed some memory leaks

### v2.2.0-alpha

* Added lz4 HC compression using a lzlib4 class which helps to make it in a similar way to zlib
* Added lz4 repository as submódule and updated Makefile
* Refactorized analisys, encodong and decoding processes
* Fixed a bug in header write/read
* Fixed LZMA compression on multistream files related with some extra data created by some compressors.

### v2.1.1-alpha

* Added encoding summary

### v2.1.0-alpha

* Added lzma compression to data and audio streams
* Now zlib is a submodule
* Updated Makefile and pipelines
* Added lzma compression with extreme option
* Added XZ compression tool repository as submodule
* Updated specifications document
* Fixed a typo in program version
* Cleaned common.h

### v2.0.3a

* Migrated to github. I have added some workflows to compile the binaries.
* Unified audio and data compressions enum
* Fixed some bugs on audio compression using zlib
* Splitted clean sector function into several functions

### v2.0.2a

* Updated changelog and readme files
* Added zlib compression to audio stream
* Fixed a bug when there are more than one stream with compression enabled


### v2.0.1a

* Little bugfixes

### v2.0.0a

* New ECM file version. This version improves the way the data is stored to allow better random access
* Zlib compression is fully working
* Added a "block size" header to help to the random seek decoder to know how many sectors exists in every block
* Added compression level option to program
* Updated help message
* Fixed a bug in stream change detection

### v1.1.0

* Changed the manual IN/OUT buffers to C File buffers. The speed is almost the same (6s to 7s), but are much easier to manage.
* I have decided to keep the C style File because was faster in several tests.
* Fixed the streams data. The correct data is now stored (counter, compression and type).
* Streams size is used in decoding (first step to add compression)
* Fixed streams data position. Now it stores the end position in the original file to be used as control.

### v1.0.5

* Fixed a bug detecting the MODE2 XA sectors which causes bigger files.
* Added the decoding functions. Now the program is able to encode and decode files
* Fixed CRC calculation on encoding
* Added CRC to verify the file integrity after decoding
* Changed some class methods to public because are required in CRC and decoding process
* Added the original progress indicator.
* Now the program also creates an stream information to be used in the future
* The original EOD indicator creates a 5 bytes data. Changed to 0x00 that only uses 1 byte.
* Some fixes
* Because the limitations of the original ECM code for the features I want, I have decided to recreate the whole program. It still based in original ECM code, but a lot of things were changed.
* Renamed to ecmtool and added some arguments. To have two binaries is not required anymore.
* The program autodetects the input format and it will decides automatically if must encode or decode the file.
* Modified Makefile to create a Win64 binary too (cross compile)
* Added automatic pipelines to bitbucket to build the releases automatically
* A new format is created because the new features are not compatible with the old format.

### v1.0.3

First release based in the original ECM program. For now is not fully working.