# ecm-tool reloaded

Error Code Modeler Reloaded

Based in the Neill Corlett ECM

By Daniel Carrasco (https://www.electrosoftcloud.com)

# Usage

##### ECMify

        ecmtool -i foo.bin
        ecmtool -i foo.bin -o bar.bin.ecm

##### UnECMify

        ecmtool -i foo.bin.ecm
        ecmtool -i foo.bin.ecm -i bar.bin

# Features

* Improved sector detection. Now it detects and removes GAP and Dummy sectors (zeroed data).

# Changelog

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

# To-Do

* Add the decode process
* ~~Improve the sector mode detection, to detect all known CD-ROM/CDDA sectors~~
* ~~Add buffered output to write in chunks and speed up the process~~
* Try to use mmap instead manual buffers to see if works faster.
* Add options to compress the stream directly instead to have to use external programs
* Convert the program into a library to make it usable by another programs (for now is a class).
* Try to ~~detect and~~ compress CDDA tracks using FLAC, APE, or similar.