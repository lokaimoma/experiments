import os
import struct
import zlib

OUTPUT_PAK = "compressed.pak"
INPUT_DIR = "./test_files"

def create_compressed_pak(input_dir, output_path):
    files = [f for f in os.listdir(input_dir) if os.path.isfile(os.path.join(input_dir, f))]
    file_count = len(files)

    toc_size = 0
    file_entries = []
    
    for filename in files:
        filepath = os.path.join(input_dir, filename)
        with open(filepath, "rb") as sf:
            raw_data = sf.read()
            
        uncompressed_size = len(raw_data)
        compressed_data = zlib.compress(raw_data, level=9)
        compressed_size = len(compressed_data)
        
        name_bytes = filename.encode('utf-8')
        
        file_entries.append({
            "name": name_bytes,
            "compressed_size": compressed_size,
            "uncompressed_size": uncompressed_size,
            "data": compressed_data
        })
        
        # 1 byte (length) + filename len + 4 (offset) + 4 (comp_size) + 4 (uncomp_size)
        toc_size += 1 + len(name_bytes) + 4 + 4 + 4

    # header (8 bytes) + toc_size
    data_offset = 8 + toc_size

    for entry in file_entries:
        entry["offset"] = data_offset
        data_offset += entry["compressed_size"]

    with open(output_path, "wb") as f:
        f.write(b"PAK1")
        f.write(struct.pack("<I", file_count))

        for entry in file_entries:
            name_bytes = entry["name"]
            f.write(struct.pack("B", len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack("<I", entry["offset"]))
            f.write(struct.pack("<I", entry["compressed_size"]))
            f.write(struct.pack("<I", entry["uncompressed_size"]))

        for entry in file_entries:
            f.write(entry["data"])

    print(f"Successfully generated {output_path} with compressed data!")

if __name__ == "__main__":
    os.makedirs(INPUT_DIR, exist_ok=True)
    create_compressed_pak(INPUT_DIR, OUTPUT_PAK)
