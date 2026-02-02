#!/usr/bin/env python3
"""
Homepage Update Script: *-browser.org -> wispbrowser.com

Rules:
1. Replace 'wispbrowser.com', 'wispbrowser.com', 'wispbrowser.com' with 'wispbrowser.com'.
2. Constraint: Do NOT change if line contains "Copyright" (case-insensitive).
3. Constraint: Do NOT change if preceded by '@' (email address).
"""

import os
import re
import argparse

# Regex: negative lookbehind for @, then the domain
DOMAIN_REGEX = re.compile(r'(?<!@)\b(?:netsurf|neosurf|wisp)-browser\.org\b', re.IGNORECASE)

def is_copyright_line(line):
    return 'copyright' in line.lower()

def process_file(path, dry_run=False):
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        original = f.read()
    
    lines = original.splitlines(keepends=True)
    new_lines = []
    changes = False
    
    for line in lines:
        if is_copyright_line(line):
            new_lines.append(line)
            continue
            
        new_line = DOMAIN_REGEX.sub('wispbrowser.com', line)
        if new_line != line:
            changes = True
        new_lines.append(new_line)
        
    if changes:
        print(f"Updating: {os.path.relpath(path)}")
        if not dry_run:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(''.join(new_lines))

def should_ignore(path):
    parts = path.split(os.sep)
    if '.git' in parts: return True
    if 'build' in parts: return True
    if 'build-ninja' in parts: return True
    if 'build-gcc' in parts: return True
    if '__pycache__' in parts: return True
    if 'utils' in parts and 'update_homepage.py' in parts: return True
    return False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    
    root_dir = os.getcwd()
    
    for root, dirs, files in os.walk(root_dir):
        # Prune dirs
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        if 'build' in dirs: dirs.remove('build')
        
        for file in files:
            full_path = os.path.join(root, file)
            if should_ignore(full_path):
                continue
                
            try:
                process_file(full_path, args.dry_run)
            except Exception as e:
                print(f"Error processing {os.path.relpath(full_path)}: {e}")

if __name__ == '__main__':
    main()
