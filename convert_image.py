from PIL import Image

def quantize_color(r, g, b):
    """Snap JPEG-compressed colors back to clean palette."""
    # Black / very dark
    if r < 30 and g < 30 and b < 30:
        return (0, 0, 0)
    # Red dominant (red bars and text) - R much larger than G and B
    if r > 80 and r > g * 2 and r > b * 2:
        return (255, 0, 0)
    # Yellow/amber dominant (grid lines) - R and G both high, B low
    if r > 80 and g > 40 and b < r * 0.5:
        return (255, 200, 0)   # warm amber/yellow
    # Fallback: keep as-is
    return (r, g, b)

def convert_to_rgb565_header(image_path, output_path, array_name, target_w, target_h):
    img = Image.open(image_path).convert("RGB")
    img = img.resize((target_w, target_h), Image.NEAREST)
    width, height = img.size
    pixels = img.load()

    with open(output_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n")
        f.write(f"#define {array_name.upper()}_HEIGHT {height}\n")
        f.write(f"#define {array_name.upper()}_WIDTH {width}\n\n")
        f.write(f"// array size is {width * height * 2}\n")
        f.write(f"static const uint16_t {array_name}[] PROGMEM = {{\n  ")

        count = 0
        for y in range(height):
            for x in range(width):
                r, g, b = pixels[x, y]
                r, g, b = quantize_color(r, g, b)
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                f.write(f"0x{rgb565:04X}, ")
                count += 1
                if count % 16 == 0:
                    f.write("\n  ")
        f.write("\n};\n")

    print(f"Done: {width}x{height} -> {output_path}")

convert_to_rgb565_header(
    "StayOnTarget320.png",
    "include/StayOnTarget320.h",
    "StayOnTarget320",
    320, 240
)
