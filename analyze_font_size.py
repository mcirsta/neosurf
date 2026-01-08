#!/usr/bin/env python3
"""
Analyze font size difference between two rendered text images.
"""
from PIL import Image
import numpy as np

def analyze_text_height(image_path):
    """Measure the height of text in an image by detecting non-white pixels."""
    img = Image.open(image_path)
    
    # Convert to RGB if needed
    if img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Convert to numpy array
    pixels = np.array(img)
    
    # Find non-white pixels (assuming white background)
    # Consider pixels non-white if any channel is < 250
    non_white = np.any(pixels < 250, axis=2)
    
    # Find rows that contain text
    rows_with_text = np.any(non_white, axis=1)
    text_rows = np.where(rows_with_text)[0]
    
    if len(text_rows) == 0:
        return 0, 0, 0
    
    top = text_rows[0]
    bottom = text_rows[-1]
    height = bottom - top + 1
    
    # Also measure width for reference
    cols_with_text = np.any(non_white, axis=0)
    text_cols = np.where(cols_with_text)[0]
    width = text_cols[-1] - text_cols[0] + 1 if len(text_cols) > 0 else 0
    
    return height, width, img.size

# Analyze both images
neosurf_path = "/home/marius/.gemini/antigravity/brain/aaa092f8-7c53-4a73-b32e-ba350d7a0866/uploaded_image_0_1767826467595.png"
chrome_path = "/home/marius/.gemini/antigravity/brain/aaa092f8-7c53-4a73-b32e-ba350d7a0866/uploaded_image_1_1767826467595.png"

print("Analyzing font sizes...")
print("=" * 60)

neosurf_height, neosurf_width, neosurf_size = analyze_text_height(neosurf_path)
chrome_height, chrome_width, chrome_size = analyze_text_height(chrome_path)

print(f"\nImage 1 (likely Neosurf):")
print(f"  Image dimensions: {neosurf_size[0]}x{neosurf_size[1]} px")
print(f"  Text height: {neosurf_height} px")
print(f"  Text width: {neosurf_width} px")

print(f"\nImage 2 (likely Chrome):")
print(f"  Image dimensions: {chrome_size[0]}x{chrome_size[1]} px")
print(f"  Text height: {chrome_height} px")
print(f"  Text width: {chrome_width} px")

print(f"\nDifference:")
height_diff = chrome_height - neosurf_height
width_diff = chrome_width - neosurf_width
height_ratio = chrome_height / neosurf_height if neosurf_height > 0 else 0

print(f"  Height difference: {height_diff:+d} px ({height_ratio:.2f}x)")
print(f"  Width difference: {width_diff:+d} px")
print(f"\nChrome text is {height_ratio:.1f}x larger than Neosurf")
