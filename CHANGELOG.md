# Changelog

### 20210707

* Added zlib compression to audio stream
* Fixed a bug when there are more than one stream with compression enabled

### 20210706

* Added a "block size" header to help to the random seek decoder to know how many sectors there are in every block
* Added compression level option to program
* Fixed a bug in zlib compression in encoding phase
* Fixed a bug in zlib compression in decoding phase
* Now zlib compression is fully working
* Updated help message
* Fixed a bug in stream change detection

### 20210705

* Added zlib compression to decoding (Not working yet because need more fixes)

### 20210704

* New ECM file version. This version improves the way the data is stored to allow better random access
* Added zlib compression to encoding (Not working yet because must be implemented in decoding too)

### 20210627

* Changed the manual IN/OUT buffers to C File buffers. The speed is almost the same (6s to 7s), but are much easier to manage.
* I have decided to keep the C style File because was faster in several tests.

### 20210618

* Fixed the streams data. The correct data is now stored (counter, compression and type).
* Streams size is used in decoding (first step to add compression)

### 20210617

* Fixed streams data position. Now it stores the end position in the original file to be used as control.

### 20210610

* Fixed a bug detecting the MODE2 XA sectors which causes bigger files. At least this bug has helped me to test the Mode 2 conversion.
* Added an option to generate an analysis about which sectors are detected, and the copied data size

### 20210609

* Added the decoding functions. Now the program is able to encode and decode files
* Fixed CRC calculation on encoding

### 20210608

* Added CRC to verify the file integrity after decoding
* Changed some class methods to public because are required in CRC and decoding process

### 20210607

* Added the progress indicator based in the original indicator.
* Now the program also creates an stream information to be used in the future
* The original EOD indicator creates a 5 bytes data. Changed to 0x00 that only uses 1 byte.
* Some fixes

### 20210606

* Because the limitations of the original ECM code for the features I want, I have decided to recreate the whole program. It still based in original ECM code, but a lot of things were changed.
* Renamed to ecmtool and added some arguments. To have two binaries is not required anymore.
* The program autodetects the input format and it will decides automatically if must encode or decode the file.
* Modified Makefile to create a Win64 binary too (cross compile)
* Added automatic pipelines to bitbucket to build the releases automatically
* A new format is created because the new features are not compatible with the old format.
* For now the program only encodes the input file. I am working in the decode phase.
