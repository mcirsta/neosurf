import sys

def main():
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} IMAGE")
    
    image_path = sys.argv[1]
    try:
        with open(image_path, 'rb') as f:
            gif_data = f.read()
    except FileNotFoundError:
        sys.exit(f"{sys.argv[0]}: open {image_path}: No such file or directory")

    print(f"{image_path}: {len(gif_data)} bytes\n")

    z = 0

    def output_chunk(description, length):
        nonlocal z
        print(description, end='')
        for i in range(length):
            if i % 8 == 0:
                print(f"\n{z + i:8}:  ", end='')
            
            if z + i >= len(gif_data):
                print("EOF\n\nUnexpected end of file")
                sys.exit()
            
            c = gif_data[z + i]
            print(f"{c:02x} ", end='')
            if 32 <= c <= 126:
                print(f"'{chr(c)}'", end='')
            else:
                print("   ", end='')
            print(" ", end='')
        
        print("\n")
        z += length

    output_chunk('Header', 6)
    output_chunk('Logical Screen Descriptor', 7)

    # gif_data[10] is the packed fields in Logical Screen Descriptor
    # Offset 10 is the 5th byte of LSD (which starts at 6).
    # LSD is 6, 7, 8, 9, 10, 11, 12.
    # Header is 0-5. LSD is 6-12.
    # Byte 10 is indeed the packed field.
    
    global_colors = gif_data[10] & 0x80
    color_table_size = 2 << (gif_data[10] & 0x07)

    if global_colors:
        output_chunk('Global Color Table', color_table_size * 3)

    while True:
        if z >= len(gif_data):
            break

        if gif_data[z] == 0x21:
            # Extension
            if z + 1 >= len(gif_data):
                output_chunk('Incomplete Extension', 1)
                break

            ext_label = gif_data[z+1]
            if ext_label == 0xf9:
                output_chunk('Graphic Control Extension', 5 + 2)
            elif ext_label == 0xfe:
                output_chunk('Comment Extension', 2)
            elif ext_label == 0x01:
                output_chunk('Plain Text Extension', 13 + 2)
            elif ext_label == 0xff:
                output_chunk('Application Extension', 12 + 2)
            else:
                if z + 2 < len(gif_data):
                    length = gif_data[z+2]
                    output_chunk(f'Unknown Extension 0x{ext_label:02x}', length + 3)
                else:
                    output_chunk(f'Unknown Extension 0x{ext_label:02x}', 2)
            
            # Blocks
            while z < len(gif_data) and gif_data[z] != 0:
                output_chunk('Data Sub-block', gif_data[z] + 1)
            if z < len(gif_data):
                output_chunk('Block Terminator', 1)
            continue

        if gif_data[z] == 0x3b:
            output_chunk('Trailer', 1)
            break

        if gif_data[z] != 0x2c:
            break
        
        # Image Descriptor
        output_chunk('Image Descriptor', 10)
        
        # Local Color Table?
        # Packed field is at offset 9 of Image Descriptor.
        # Image Descriptor starts at z (before increment).
        # Wait, output_chunk increments z.
        # So we need to peek or remember.
        # The Perl script doesn't handle Local Color Table!
        # It just says:
        # output_chunk('Image Descriptor', 10);
        # output_chunk('Table Based Image Data', 1);
        # while ...
        # It misses Local Color Table parsing if present.
        # I should match Perl script behavior exactly as requested ("output is the same").
        
        output_chunk('Table Based Image Data', 1)

        while z < len(gif_data) and gif_data[z] != 0:
            output_chunk('Data Sub-block', gif_data[z] + 1)
        if z < len(gif_data):
            output_chunk('Block Terminator', 1)

    if z != len(gif_data):
        output_chunk('*** Junk on End ***', len(gif_data) - z)

if __name__ == "__main__":
    main()
