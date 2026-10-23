import os
import struct

OUTPUT_PAK = "test.pak"
INPUT_DIR = "./test_files"  # Put some dummy text/image files in this folder

def create_pak(input_dir, output_path):
    files = [f for f in os.listdir(input_dir) if os.path.isfile(os.path.join(input_dir, f))]
    file_count = len(files)

    # 1. Calculate TOC size to determine where data blobs start
    toc_size = 0
    file_entries = []
    
    for filename in files:
        filepath = os.path.join(input_dir, filename)
        filesize = os.path.getsize(filepath)
        name_bytes = filename.encode('utf-8')
        assert len(name_bytes) < 256, "Filename too long for uint8 length prefix!"
        
        file_entries.append({
            "name": name_bytes,
            "size": filesize,
            "path": filepath
        })
        # 1 byte (length) + filename len + 4 bytes (offset) + 4 bytes (size)
        toc_size += 1 + len(name_bytes) + 4 + 4

    # Header (4 bytes magic + 4 bytes count) = 8 bytes
    data_offset = 8 + toc_size

    # Update absolute offsets for each file data blob
    for entry in file_entries:
        entry["offset"] = data_offset
        data_offset += entry["size"]

    # 2. Write the .pak file
    with open(output_path, "wb") as f:
        # Write Header
        f.write(b"PAK1")
        f.write(struct.pack("<I", file_count))

        # Write TOC
        for entry in file_entries:
            name_bytes = entry["name"]
            f.write(struct.pack("B", len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack("<I", entry["offset"]))
            f.write(struct.pack("<I", entry["size"]))

        # Write Data Blobs
        for entry in file_entries:
            with open(entry["path"], "rb") as sf:
                f.write(sf.read())

    print(f"Successfully generated {output_path} with {file_count} files!")

if __name__ == "__main__":
    os.makedirs(INPUT_DIR, exist_ok=True)
    create_pak(INPUT_DIR, OUTPUT_PAK)
