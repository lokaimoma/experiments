# Custom `.pak` Archive Parser & Index Reader

A step up from `../tga_image_header_reader` where I worked on parsing just a single fixed-size header.
Here, we work on a container format (`.pak`) with a table of content.

## Scope

C++ parser for a custom .pak binary archive format. The goal is to read a structured file index
(Table of Contents), resolve file offsets and sizes, and extract embedded data blobs using random
access stream navigation.

# File Format Specification (.pak)

## Archive Header (8 Bytes)

- Magic Signature (4 bytes): ASCII bytes PAK1 (0x50 0x41 0x4B 0x31) used to validate the filetype.
- File Count (4 bytes, Little-Endian): The total number of files indexed within the archive.

## Table of Contents (Repeated `file_Count` Times)

- Filename Length (a byte): Size of the filename
- Filename (variable bytes): Variable-size character array storing the name/path of the archived file.
- File Offset (4 bytes, Little-Endian): The absolute byte offset from the start of the file where the data blob begins.
- File Size (4 bytes, Little-Endian): The exact length of the file data blob in bytes.

## Data Blobs Region

- Raw byte streams of the individual files packed end-to-end, accessed dynamically via their TOC offsets.

# Key Technical Concepts & Architecture

- Container Indexing: Reading a dynamic number of records indicated by a header variable rather than relying on a fixed-size header.
- Random File Access: Moving the file stream pointer dynamically between the Table of Contents index and distant data offsets.
- Stream Deserialization: Reusing custom little-endian helper functions (e.g: read_uint32) to safely parse multibyte integers across multi-platform systems.
- Struct Packing & Alignment: Designing clean metadata structures to represent archive entries in memory.

# Implementation Milestones

- [x] Phase 1: Header Validation — Open file in binary mode, read and verify the PAK1 magic bytes, and extract the file_count.
- [x] Phase 2: TOC Iteration — Loop through the archive index to parse every file's name, offset, and size into a structured vector.
- [x] Phase 3: Blob Extraction — Use seekg to jump to a specific file's offset and dump its raw contents to disk or memory.

## Testing

Put some sample files in `scripts/test_files/`, then generate an archive and test it:

```sh
cd scripts
python3 pak_generator.py
cd ..
./build/cpkg_reader_mtester scripts/test.pak list
```

Use `info` instead of `list` for archive details, or `extract <index> [output-path]` to extract a file.
