# fat-image-reader

Read a FAT12/FAT16/FAT32 formatted image file.

## Usage

Compile `fat.c` and run the command.

```sh
fat IMAGE_FILE [...FILE]
```

The example loads `foo.img` and starts interactive reading.

```sh
fat foo.img
```

The example loads `foo.img` and shows the info and contents of `/bar.txt` in the image.

```sh
fat foo.img /bar.txt
```
