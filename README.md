# ecm-tool reloaded

Error Code Modeler Reloaded

Based in the Neill Corlett ECM

By Daniel Carrasco (https://www.electrosoftcloud.com)

# Description

This ECM Tool allows you to remove all recoverable data from CD-ROM sectors, reducing the image size about 20% without using compression (depending of sectors types). This will allow to read the data from it very fast, and also can be complemented with a compression tool like 7zip to reach higher compression ratios.

This tool is completely loossless, so original data can be recovered without problems.

In the v2 versions, internal compression was added and then external tools are not required anymore. ECM Tool Reloaded is also able to detect the stream type. This make possible to use FLAC compresison in an Audio stream, which compress much more than other compression methods like LZMA or ZLIB.

# Compression tests

This tests were made using the original ECM tool made by Neill Colett, this ECM Tool Reloaded without compression (just EC removal), 7zip extreme compression, Original ECM tool + 7zip extreme compression and this ECM Tool Reloaded using its internal compression method (LZMA for data and FLAC for audio, both in extreme compression mode).

|            | Original Size |    Original ECM   | ECM Tool Reloaded |        7zip       | Original ECM + 7z | ECM Tool Reloaded (Compressed) |
|:----------:|:-------------:|:-----------------:|:-----------------:|:-----------------:|:-----------------:|:------------------------------:|
| FFVIII CD3 |  721.508.928  | 635.001.493 (12%) | 574.742.104 (20%) | 438.608.753 (39%) | 365.071.928 (49%) |        362.903.256 (50%)       |
| Firebugs   |  608.401.248  |  597.995.580 (2%) |  560.062.104 (8%) | 498.312.033 (18%) | 493.397.731 (19%) |        363.280.352 (40%)       |
| Tekken 3   |  687.922.368  |  645.620.932 (6%) | 606.047.556 (12%) | 457.049.639 (34%) | 426.481.504 (38%) |        417.325.163 (39%)       |


As you can see, the ECM Tool Reloaded uncompressed size is improved a bit by removing more data and dummy sectors from the streams. The CDDA tracks in Firebugs have caused a poor reduction ratio in original ECM tool because it keeps all the sectors. Reloaded conversely removes the GAP data, so  reduction ratio is improved a bit.

About the compressed versions, the 7zip standalone compression is as expected the worst of all. The compression ratio if LZMA compressor was improved by the Original ECM Tool thanks to the removed data. The Reloaded version improves notably the CDDA compression ratio thanks to the FLAC compression, but is similar to the Original ECM Tool + 7zip if the image doesn't contains any CDDA track (it is even a bit worst). This maybe can be improved by using bigger blocks during the compression step, but will need more memory for the process.

# Usage

```
To encode:
    ecmtool -i/--input cdimagefile
    ecmtool -i/--input cdimagefile -o/--output ecmfile

To decode:
    ecmtool -i/--input ecmfile
    ecmtool -i/--input ecmfile -o/--output cdimagefile

Optional options:
    -a/--acompression <zlib/lzma/lz4/flac>
           Enable audio compression
    -d/--dcompression <zlib/lzma/lz4>
           Enable data compression
    -c/--clevel <0-9>
           Compression level between 0 and 9
    -e/--extreme-compression
           Enables extreme compression mode for LZMA/FLAC (can be very slow)
    -s/--seekable
           Create a seekable file. Reduce the compression ratio but
           but allow to seek into the stream.
    -p/--sectors_per_block <sectors>
           Add a end of block mark every X sectors in a seekable file. Max 255.
    -f/--force
           Force to ovewrite the output file
```

# Features

* Improved sector detection. Now it detects and removes GAP and Dummy sectors (zeroed data).
* Removes almost all non data bytes from sectors (sync, addr, mode, 50% of flags, EDC and ECC)
* A bit faster encoding/decoding: About 8s to encode or decode the FFVIII Disk 1 vs 11-14s of the original ECM tool.
* Internal zlib, lzma, lz4 and FLAC compressions to do not depend of external tools.
* Contains a sectors TOC in header and sectors sizes are constant, so it can be easily indexed.

# Changelog

Changelog can be viewed in [CHANGELOG.md](CHANGELOG.md) file

# FAQ

## How this program works

CDROM data is splitted in sectors, and those sectors are of 2352 bytes size. Depending of the sector type, it may contains Error Detection Code and Error Correction Code data, which will help to the CDROM drive to detect and recover a sector if it is damaged (for example by dust or even little scratches). This data is generated during the burn process and can be easily generated using an algorithm, and like it is not usefull to keep the data outside a CDROM, can be removed. On the decoding process the program will generate that data again and then the image file will be exactly like the original (checked using MD5, SHA1...).

You can find more info in the [CDROM Rainbow books](https://en.wikipedia.org/wiki/Rainbow_Books) 

## Why I have created this "Reloaded" version

The original program created by Neill Corlett is a very good tool... I was using it for a long time to store my CDROM images collection. The problem is that original ecm tool process the file without a clear block separation, storing the sectors info scattered in file. That makes the file unseekable or at least very hard to be seeked, and that is why my program was created.

The ECM2 format keeps the sectors data in header, so just reading that data you can generate an index to seek in file very fast. Then you just will have to read the sector data depending of which type is, and regenerate the removed data easily. I think that this will be usefull for emulators and I am planning to do some tests in the future.

ECM2 format also removes more data than original ECM format (depending of sector type, of course), like MSF if not libcrypt protection exists in game, sync, redundant flags, and zeroed sectors. This will help to save up to 8% of the image size.

Finally, ECM2 format includes compression support which not only will allow to do everything in the same program, also will allow to compress every stream type with a different method, getting higher compression ratios in images that contains CDDA tracks thanks to the FLAC algorithm. The best example is the above Firebugs comparison in compression tests.

## Can be used with Audio CD images

Of course!... but the resulting file will be just the compressed stream if compression is used, and the whole data if not. That data will be packed in a ECM2 file format, which make it unplayable using an Audio Player program (even if FLAC compression is used). The best for that kind of images is to use an Audio CD ripper which allows you to create FLAC or another format files, which can be easily played on an Audio Player.

## Why it still in Alpha

I still working in this tool, so the ECM2 format can suffer changes. Also the tool is not extensive tested and there are bugs that must be fixed before remove the Alpha status. Just now I am perfoming several tests, and on those tests I have located two bugs (one of them was critical).