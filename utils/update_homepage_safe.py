#!/usr/bin/env python3
"""
Homepage Update Script (Safe): *-browser.org -> wispbrowser.com

Rules:
1. CODE: Replace 'wisp-browser.org', 'netsurf-browser.org', 'neosurf-browser.org' with 'wispbrowser.com'.
2. COMMENTS: Do NOT touch anything. Returns exact original text.
"""

import os
import re
import argparse
import sys

# Extensions mapping to comment style
EXT_MAP = {
    # C-Style: // and /* */
    '.c': 'c', '.cpp': 'c', '.h': 'c', '.hpp': 'c', '.cc': 'c',
    '.js': 'c', '.ts': 'c', '.java': 'c', '.css': 'c', '.scss': 'c',
    '.go': 'c', '.rs': 'c', '.php': 'c', '.swift': 'c',
    
    # Python-Style: # (Docstrings treated as strings usually, but we'll simplistic parse)
    '.py': 'py', '.sh': 'py', '.bash': 'py', '.yaml': 'py', '.yml': 'py',
    '.cmake': 'py', '.txt': 'py', 'makefile': 'py', '.mk': 'py',
    '.conf': 'py', '.ini': 'py', '.cfg': 'py', '.toml': 'py',
    
    # HTML-Style: <!-- -->
    '.html': 'html', '.xml': 'html', '.svg': 'html', '.htm': 'html'
}

# Regex for replacements
# 1. wisp-browser.org (or netsurf/neosurf) -> wispbrowser.com
#    Avoid emails: (?<!@)
RE_DOMAIN = re.compile(r'(?<!@)\b(?:wisp-browser|netsurf-browser|neosurf-browser)\.org\b', re.IGNORECASE)

# 2. wisp-browser -> wispbrowser (generic, if not covered above)
#    Avoid emails.
RE_PROJECT_NAME = re.compile(r'(?<!@)\bwisp-browser\b', re.IGNORECASE)

def replace_token(text):
    """
    Code replacement logic.
    """
    original = text
    
    # First replace full domains
    text = RE_DOMAIN.sub("wispbrowser.com", text)
    
    # Then replace generic project name if lingering
    text = RE_PROJECT_NAME.sub("wispbrowser", text)
    
    return text

def process_code_part(text):
    """
    Process a code segment.
    """
    return replace_token(text)

def process_comment_text(text):
    """
    Process text identified as a comment.
    Rule: Do NOT touch.
    """
    return text

def process_file_content(content, ext):
    """
    Parse content based on extension and apply rules.
    Returns (new_content, changed_bool)
    """
    style = EXT_MAP.get(ext, 'c') # Default to C-style for unknown
    if os.path.basename(ext).lower() in ['makefile', 'cmakelists.txt', 'dockerfile']:
        style = 'py'
    
    lines = content.splitlines(keepends=True)
    new_lines = []
    changes_made = False
    
    in_block_comment = False
    
    for line in lines:
        original_line = line
        
        if style == 'c':
            line, in_block_comment = process_c_line(line, in_block_comment)
        elif style == 'py':
            line = process_py_line(line)
        elif style == 'html':
            line, in_block_comment = process_html_line(line, in_block_comment)
        else:
            line = process_generic_line(line)
            
        if line != original_line:
            changes_made = True
            
        new_lines.append(line)
        
    return ''.join(new_lines), changes_made

def process_c_line(line, in_block_comment):
    # If we are inside a /* ... */ block
    if in_block_comment:
        if '*/' in line:
            # End of block
            pre, post = line.split('*/', 1)
            new_pre = process_comment_text(pre + '*/')
            new_post, still_in_block = process_c_line(post, False) 
            return new_pre + new_post, still_in_block
        else:
            # Still in block comment
            return process_comment_text(line), True

    # Check for //
    if '//' in line:
        idx = line.find('//')
        # Check valid comment quotes check
        pre_comment = line[:idx]
        if pre_comment.count('"') % 2 == 0 and pre_comment.count("'") % 2 == 0:
            # It IS a comment
            code_part = pre_comment
            comment_part = line[idx:]
            
            new_code = process_code_part(code_part)
            new_comment = process_comment_text(comment_part)
            return new_code + new_comment, False

    # Check for /* (Start of block)
    if '/*' in line:
        idx = line.find('/*')
        pre_comment = line[:idx]
        if pre_comment.count('"') % 2 == 0 and pre_comment.count("'") % 2 == 0:
            code_part = pre_comment
            rest = line[idx:]
            
            # Does it end on same line?
            if '*/' in rest:
                # /* ... */ inline
                comment_end_idx = rest.find('*/') + 2
                comment_part = rest[:comment_end_idx]
                rem_code = rest[comment_end_idx:]
                
                new_code = process_code_part(code_part)
                new_comment = process_comment_text(comment_part)
                # Recurse for the remainder
                new_rem, _ = process_c_line(rem_code, False)
                return new_code + new_comment + new_rem, False
            else:
                # Starts block comment
                new_code = process_code_part(code_part)
                new_comment = process_comment_text(rest)
                return new_code + new_comment, True

    # Code only
    return replace_token(line), False

def process_py_line(line):
    # Python style: # comments
    if '#' in line:
        idx = line.find('#')
        pre_comment = line[:idx]
        # Quote check
        if pre_comment.count('"') % 2 == 0 and pre_comment.count("'") % 2 == 0:
            code_part = pre_comment
            comment_part = line[idx:]
            
            new_code = process_code_part(code_part)
            new_comment = process_comment_text(comment_part)
            return new_code + new_comment
            
    return replace_token(line)

def process_html_line(line, in_block_comment):
    # HTML comments <!-- ... -->
    if in_block_comment:
        if '-->' in line:
            pre, post = line.split('-->', 1)
            new_pre = process_comment_text(pre + '-->')
            new_post, still_in_block = process_html_line(post, False)
            return new_pre + new_post, still_in_block
        else:
            return process_comment_text(line), True

    if '<!--' in line:
        idx = line.find('<!--')
        code_part = line[:idx]
        rest = line[idx:]
        
        if '-->' in rest:
            end_idx = rest.find('-->') + 3
            comment_part = rest[:end_idx]
            rem_code = rest[end_idx:]
            
            new_code = process_code_part(code_part)
            new_comment = process_comment_text(comment_part)
            new_rem, _ = process_html_line(rem_code, False)
            return new_code + new_comment + new_rem, False
        else:
            new_code = process_code_part(code_part)
            new_comment = process_comment_text(rest)
            return new_code + new_comment, True
            
    return replace_token(line), False

def process_generic_line(line):
    # For generic files, we don't know comment style, so we process as code.
    # But usually these are docs or similar.
    return replace_token(line)

def should_ignore(path):
    parts = path.split(os.sep)
    if '.git' in parts: return True
    if 'build' in parts: return True
    if 'build-ninja' in parts: return True
    if 'build-gcc' in parts: return True
    if '__pycache__' in parts: return True
    if 'utils' in parts and 'update_homepage_safe.py' in parts: return True
    
    filename = os.path.basename(path)
    if filename.startswith('COPYING') or filename.startswith('LICENSE') or filename.startswith('licence'):
        return True
        
    return False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dry-run', action='store_true', help="Don't write changes")
    args = parser.parse_args()
    
    root_dir = os.getcwd()
    
    targets = []
    for root, dirs, files in os.walk(root_dir):
        # prune directories
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        if 'build' in dirs: dirs.remove('build')
        
        for file in files:
            full_path = os.path.join(root, file)
            if not should_ignore(full_path):
                targets.append(full_path)
    
    print(f"Checking {len(targets)} files...")
    
    for file_path in targets:
        try:
            # Attempt to read as UTF-8
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            ext = os.path.splitext(file_path)[1].lower()
            if os.path.basename(file_path) == 'Makefile': ext = 'makefile'
            
            new_content, changed = process_file_content(content, ext)
            
            if changed:
                print(f"Modified: {os.path.relpath(file_path)}")
                if not args.dry_run:
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(new_content)
        except Exception as e:
            print(f"Skipping {os.path.relpath(file_path)}: {e}")

if __name__ == '__main__':
    main()
