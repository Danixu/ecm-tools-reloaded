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
        ecmtool -i foo.bin.ecm -o bar.bin

# Features

* Improved sector detection. Now it detects and removes GAP and Dummy sectors (zeroed data).
* Removes almost all non data sectors (sync, addr, mode, 50% of flags, EDC and ECC)
* Faster encoding/decoding: About 6s to encode or decode the FFVIII Disk 1 vs 11-14s of the original ECM tool.
* The important functions works over streams, so are easier to implement on another tools.

# Changelog

Changelog can be viewed in file [CHANGELOG.md](CHANGELOG.md)

# To-Do

* ~~Add the decode process~~
* ~~Improve the sector mode detection, to detect all known CD-ROM/CDDA sectors~~
* ~~Add buffered output to write in chunks and speed up the process~~
* Try to use mmap instead manual buffers to see if works faster.
* Add options to compress the stream directly instead to have to use external programs
* Convert the program into a library to make it usable by another programs (for now is a class).
* Try to ~~detect and~~ compress CDDA tracks using FLAC, APE, or similar.
* Verify that all CD-ROM modes are working (For now only CDDA, Mode 2 and Mode 2 XA 1 were tested)