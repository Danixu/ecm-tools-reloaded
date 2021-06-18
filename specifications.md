# ECM Reloaded specifications

The ECM Reloaded file specifications are incompatible with the original ECM version. This version only works with CD-ROM images and manage the data in a different way.

## File specification

The ECM reloaded file specifications are the following.

| Data type   | Position      | Description                                                                                   |
|-------------|---------------|-----------------------------------------------------------------------------------------------|
| Header      | 0x00 - 0x03   | The file header consisting in the "ECM" text followed by a 0x01 character                     |
| Streams TOC | 0x04 - ??     | Information about the stream and their compression, ended by 0x00                             |
| Sectors TOC | ?? - ??       | Information about the sectors and their types, ended by 0x00                                  |
| Data        | ?? - ??       | The data copied from the original file used to regenerate it                                  |
| EDC         | ?? - ??       | The last 4 bytes of the file is the error detection code, used to verify the file             |
-------------------------------------------------------------------------------------------------------------------------------

### Header

The header is just to identify the file. Uses the same first three bytes as the original ECM program, but the 4rd changes to 1 to indicate the file version.

### Streams TOC

The streams TOC contains all the information about the streams that were detected in the original file (audio and data), their end position in the original file and the compression used in this stream. This will be used to compress the data and the audio separately. The end positions is to verify that all bytes are readed.

Every stream in the TOC will contains the following data.

| Data type   | Position  | Description                                                                            |
|-------------|-----------|----------------------------------------------------------------------------------------|
| Stream Info | 0x00      | This byte contains the info about the stream (type, compression)                       |
| Size        | 0x01 - ?? | The size of the stream in the ecm file to detect when the program must stop reading    |

A 0x00 byte will be added at the end of the Streams TOC (At the end of all, not in every). The first bit of the Count bytes will be used to store the "continuation" byte, wich will tell the program to continue reading the next byte. This will limit the number stored in those bytes up to 128 instead the size of an uint8_t variable (256).

#### First byte

```
Bits |  0    1    2    3    4    5    6    7
      |   ||             ||             ||   |
       ---  -------------  -------------  ---
        |      Counter         Comp       Type
        |
        └--  Continuation bit
```

The first stream byte is composed by the first bit that is a continuation bit and will indicate if the program must use the next byte to the count calculation. Next three bytes are just the first count divider, the next three bytes are used to store the stream compression and the last bit will contain the stream type.

#### Next Nth bytes

```
Bits |  0    1    2    3    4    5    6    7
      |   ||                                |
       ---  --------------------------------
        |               Counter
        |
        └--  Continuation bit
```

As was explained in the above section, this byte will have he first bit as "continuation bit", and will indicate if the program must read the next byte to be used in the count calculation. The next 7 bits will be used to store the count divider.

### Sectors TOC

The sectors TOC is a list of kind of sectors detected in the original file, which will be used later to regenerate the original data based on its kind.

| Data type             | Position  | Description                                                                                                |
|-----------------------|-----------|------------------------------------------------------------------------------------------------------------|
| Sector info and count | 0x00      | The sector type and part of the count (3 bits). The count divided by 8 will be stored here                 |
| Count                 | 0x01 - ?? | The rest of the count bytes. If the sectors count is bigger than 8, will be stored in the following bytes  |

The first bit of the Count bytes will be used to store the "continuation" byte, wich will tell the program to continue reading the next byte. This will limit the number stored in those bytes up to 128 instead the size of an uint8_t variable (256). This stream also ends by 0x00 to mark the EOD.

#### First byte

```
Bits |  0    1    2    3    4    5    6    7
      |   ||             ||                 |
       ---  -------------  -----------------
        |      Counter        Sector type
        |
        └--  Continuation bit
```

First bit is 1 when the program must continue reading the next byte to be used in sectors count calculation. The last count byte will have this bit to 0. Next 3 bites are used to store the first part of the count, and last four bits are used to store the sector type.

#### Next Nth bytes

```
Bits |  0    1    2    3    4    5    6    7
      |   ||                                |
       ---  --------------------------------
        |               Counter
        |
        └--  Continuation bit
```

As was explained in the above section, this byte will have he first bit as "continuation bit", and will indicate if the program must read the next byte to be used in the count calculation. The next 7 bits will be used to store the count.

### Data

Contains the data copied from the original file, wich will depend of every sector type.

### EDC

Contains a 4 bytes data wich will be used to verify the restored file after extracting the ECM.