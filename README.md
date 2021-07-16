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
* Added zlib and lzma compression to do not depend of external tools
* The important functions works over streams, so are easier to implement on another tools.

# Changelog

Changelog can be viewed in [CHANGELOG.md](CHANGELOG.md) file