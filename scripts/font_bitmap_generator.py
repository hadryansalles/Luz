from PIL import Image, ImageDraw, ImageFont
import os

def generate_bitmap_grid(font_path, output_image_path, output_txt_path, char_resolution=(64, 64), grid_size=(16, 16)):
    font = ImageFont.truetype(font_path, char_resolution[1])
    
    image_width = char_resolution[0] * grid_size[0]
    image_height = char_resolution[1] * grid_size[1]
    image = Image.new("1", (image_width, image_height), color=1)  # "1" for 1-bit pixels, black and white
    draw = ImageDraw.Draw(image)
    
    bitmap_strings = []

    for char_code in range(256):
        char = chr(char_code)
        x = (char_code % grid_size[0]) * char_resolution[0]
        y = (char_code // grid_size[0]) * char_resolution[1]
        bbox = draw.textbbox((0, 0), char, font=font)
        width, height = bbox[2] - bbox[0], bbox[3] - bbox[1]
        draw.text((x + (char_resolution[0] - width) / 2, y + (char_resolution[1] - height) / 2), char, font=font, fill=0)
        
        char_image = image.crop((x, y, x + char_resolution[0], y + char_resolution[1]))
        char_bitmap = list(char_image.getdata())
        
        bitmap_str = ''.join(['1' if pixel == 0 else '0' for pixel in char_bitmap])
        formatted_str = f"Character: {char if char.isprintable() else ' '} (ASCII {char_code})\n" + '\n'.join([bitmap_str[i:i+char_resolution[0]] for i in range(0, len(bitmap_str), char_resolution[0])]) + "\n\n"
        bitmap_strings.append(formatted_str)
    
    image.save(output_image_path)
    print(f"Bitmap grid saved as {output_image_path}")
    
    with open(output_txt_path, 'w', encoding='utf-8') as f:
        f.writelines(bitmap_strings)
    print(f"Bitmap strings saved as {output_txt_path}")

font_path = "VT323-Regular.ttf"
output_image_path = "ascii_bitmap_grid.png"
output_txt_path = "ascii_bitmaps.txt"
generate_bitmap_grid(font_path, output_image_path, output_txt_path)
