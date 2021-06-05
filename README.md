# ecm-tools

Error Code Modeler

Original Author: Neill Corlett

Modified by: Daniel Carrasco (https://www.electrosoftcloud.com)

# Usage

##### ECMify

        ecmtool -i foo.bin
        ecmtool -i foo.bin -o bar.bin.ecm

##### UnECMify

        ecmtool -i foo.bin.ecm
        ecmtool -i foo.bin.ecm -i bar.bin


# Changelog

### 20200602

* Input and Output files are set by arguments instead to be positional
* Binary renamed to ecmtool because two binaries are not required anymore.
* The program autodetects the input format, so it will decode the ECM files and encode the binary files with the same binary.
* Modified Makefile to create a Win64 binary too
* Added automatic pipelines to bitbucket to build the releases automatically
* A new format was created keeping the compatibility with the old format. This new format is not complete yet and for now the output file is a few bytes bigger.

### 20200603

 * Now the program detects the mode 2 with header correctly.

| Game                 |Format | ECM1 size      | ECM2 size      | Space Saved |
|----------------------|-------|----------------|----------------|-------------|
| Final Fantasy 8 CD 1 |  ECM  | 657.819.103    | 653.052.304    |      1%     |
|                      |  7z   | 410.015.594    | 412.565.554    |    -0,7%    |
|                      |  zip  | 463.626.196    | 463.061.019    |     0,2%    |
|                      | bzip2 | 458.615.204    | 458.432.525    |    0.04%    | 
|                      |  gzip | 463.649.700    | 463.079.742    |     0.2%    |      

# To-Do

* ~~Improve the sector mode detection, adding detection for Mode 2 sectors with headers~~.
* Add buffered output to write in chunks and speed up the process
* Add options to compress the stream directly instead to have to use external programs
* Convert the program into a library to make it usable by another programs
* Try to detect and compress CDDA tracks using FLAC, APE, or similar.