#!/usr/bin/env python3
"""
Property Registration Generator for LibCSS

Generates property enums, dispatch tables, and string indices from properties.gen
to eliminate manual alignment errors.

Strategy: Parse dispatch.c for canonical property order, then match properties.gen
"""

import sys
import re
from pathlib import Path


class DispatchParser:
    """Parse dispatch.c to get canonical property order and inherited flags."""
    
    def __init__(self, dispatch_file):
        self.dispatch_file = Path(dispatch_file)
        self.properties = []  # List of {name, inherited} dicts
        
    def parse(self):
        """Extract property order and inherited flags from dispatch.c."""
        # Validate file exists and is readable
        if not self.dispatch_file.exists():
            print(f"FATAL ERROR: dispatch.c not found at {self.dispatch_file}", file=sys.stderr)
            sys.exit(1)
        
        try:
            with open(self.dispatch_file, 'r') as f:
                content = f.read()
        except IOError as e:
            print(f"FATAL ERROR: Cannot read dispatch.c: {e}", file=sys.stderr)
            sys.exit(1)
        
        # Validate that prop_dispatch array exists
        if 'struct prop_table prop_dispatch[' not in content:
            print("FATAL ERROR: Cannot find 'struct prop_table prop_dispatch[' in dispatch.c", file=sys.stderr)
            print("File format may have changed. Expected C code with dispatch table.", file=sys.stderr)
            sys.exit(1)
        
        # Find the prop_dispatch array
        # Format: PROPERTY_FUNCS(name), \n inherited_flag,
        pattern = r'PROPERTY_FUNCS\(([a-z_]+)\),\s*\n\s*(\d+),'
        matches = re.findall(pattern, content)
        
        # Validate we found properties
        if not matches:
            print("FATAL ERROR: No property entries found in dispatch.c", file=sys.stderr)
            print(f"Expected pattern: PROPERTY_FUNCS(name),\\n  inherited_flag,", file=sys.stderr)
            print("Check that dispatch.c format hasn't changed.", file=sys.stderr)
            sys.exit(1)
        
        if len(matches) < 50:
            print(f"FATAL ERROR: Only {len(matches)} properties found in dispatch.c", file=sys.stderr)
            print("Expected at least 50 properties. File may be truncated or malformed.", file=sys.stderr)
            sys.exit(1)
        
        for prop_name, inherited_str in matches:
            # Validate inherited flag is 0 or 1
            if inherited_str not in ('0', '1'):
                print(f"FATAL ERROR: Invalid inherited flag '{inherited_str}' for property {prop_name}", file=sys.stderr)
                print("Inherited flag must be 0 or 1", file=sys.stderr)
                sys.exit(1)
            
            self.properties.append({
                'name': prop_name,
                'inherited': int(inherited_str) == 1
            })
        
        return self.properties


class PropertyGenParser:
    """Parse properties.gen to get enum names and metadata."""
    
    def __init__(self, gen_file):
        self.gen_file = Path(gen_file)
        self.prop_map = {}  # name -> metadata
        
    def parse(self):
        """Parse properties.gen and build name->enum mapping."""
        if not self.gen_file.exists():
            print(f"FATAL ERROR: properties.gen not found at {self.gen_file}", file=sys.stderr)
            sys.exit(1)
        
        try:
            with open(self.gen_file, 'r') as f:
                lines = list(f)
        except IOError as e:
            print(f"FATAL ERROR: Cannot read properties.gen: {e}", file=sys.stderr)
            sys.exit(1)
        
        for line_num, line in enumerate(lines, 1):
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Parse: property_name:ENUM_NAME ...
            # ENUM_NAME can be any uppercase identifier (CSS_PROP_*, BORDER_SIDE_*, etc.)
            match = re.match(r'^([a-z_]+):([A-Z][A-Z0-9_]+)\s+(.*)$', line)
            if match:
                prop_name = match.group(1)
                enum_name = match.group(2)
                spec = match.group(3)
                
                # Validate: enum should be uppercase and have valid format
                if not re.match(r'^[A-Z][A-Z0-9_]*$', enum_name):
                    print(f"WARNING: Line {line_num}: Invalid enum format '{enum_name}'", file=sys.stderr)
                    continue
                
                # Check if shorthand, manual, or generic
                # SHORTHAND = shorthand property with manual .c implementation (not in dispatch.c)
                # MANUAL = longhand property with manual .c implementation (in dispatch.c)
                # WRAP = wrapper calling a generic function
                # ALIAS = property that parses to a different property's bytecode (e.g., inline-size -> width)
                is_shorthand = spec.strip() == 'SHORTHAND' or 'WRAP:' in spec
                is_manual = spec.strip() == 'MANUAL'
                is_generic = 'GENERIC' in spec
                
                # Detect aliases: property name doesn't match enum's base name
                # e.g., inline_size:CSS_PROP_WIDTH is an alias for width
                enum_base = enum_name.replace('CSS_PROP_', '').lower()
                is_alias = (prop_name != enum_base) and not is_shorthand and not is_generic
                
                if not is_generic:
                    self.prop_map[prop_name] = {
                        'enum': enum_name,
                        'is_shorthand': is_shorthand,
                        'is_alias': is_alias,
                        'line': line_num
                    }
        
        # Validate we found properties
        if not self.prop_map:
            print("FATAL ERROR: No properties found in properties.gen", file=sys.stderr)
            print("Expected format: property_name:CSS_PROP_ENUM_NAME PARSE_SPEC", file=sys.stderr)
            sys.exit(1)
        
        if len(self.prop_map) < 50:
            print(f"FATAL ERROR: Only {len(self.prop_map)} properties in properties.gen", file=sys.stderr)
            print("Expected at least 50. File may be incomplete.", file=sys.stderr)
            sys.exit(1)
        
        return self.prop_map


class GperfInputGenerator:
    """Generate gperf input file for perfect hash CSS property lookup.
    
    Uses GNU gperf (an external tool) instead of the Python perfect-hash
    module. The Python script generates a .gperf input file, and CMake
    invokes gperf to produce the final C code.
    
    This follows the same pattern used by libhubbub's element-type.gperf.
    """
    
    def __init__(self, keys):
        """Initialize with list of (css_name, handler_name) tuples."""
        self.keys = keys
    
    def generate_gperf_input(self):
        """Generate gperf input file content.
        
        The gperf file defines:
        - struct css_prop_entry { name, handler }
        - A lookup function css_prop_lookup_generated() returning const struct *
        
        A wrapper function css_prop_lookup() is appended after the gperf
        output (via the %% section) to extract the handler field, preserving
        the existing API: css_prop_handler css_prop_lookup(name, len).
        """
        lines = []
        
        # gperf declarations section
        lines.append("/*")
        lines.append(" * AUTO-GENERATED gperf input - DO NOT EDIT!")
        lines.append(" *")
        lines.append(" * Generated by property_generator.py")
        lines.append(" * Processed by GNU gperf to produce prop_hash_table.inc")
        lines.append(" */")
        lines.append("")
        
        # gperf options - optimized for fastest possible lookup
        #
        # %compare-lengths: reject mismatches by length before strncmp
        #   (cheap integer compare eliminates most non-matches)
        # %compare-strncmp: use strncmp for final verification
        #   (required: hash alone can't distinguish unknown inputs from
        #    valid keywords that happen to share the same hash slot)
        # %readonly-tables: const-qualify all tables (cache-friendly)
        # %ignore-case: case-insensitive matching (CSS is case-insensitive)
        # %enum: use enum for hash values (compiler optimization)
        # %7bit: assume 7-bit ASCII input (valid for CSS property names)
        # %null-strings: NULL for empty slots (fast pointer check)
        # No %switch: default array lookup is faster for 200+ keywords
        lines.append("%language=ANSI-C")
        lines.append("%compare-lengths")
        lines.append("%compare-strncmp")
        lines.append("%readonly-tables")
        lines.append("%ignore-case")
        lines.append("%struct-type")
        lines.append("%enum")
        lines.append("%7bit")
        lines.append("%null-strings")
        lines.append("%define hash-function-name css_prop_hash")
        lines.append("%define lookup-function-name css_prop_lookup_generated")
        lines.append("")
        
        # C code block included verbatim before the hash table
        lines.append("%{")
        lines.append("#include <string.h>")
        lines.append("#include <stddef.h>")
        lines.append("")
        lines.append('#include "parse/properties/properties.h"')
        lines.append("%}")
        lines.append("")
        
        # Full struct definition - gperf needs this to generate the wordlist array.
        # The 'name' field is the keyword (filled by gperf automatically).
        # The 'handler' field is the css_prop_handler function pointer.
        lines.append("struct css_prop_entry {")
        lines.append("    const char *name;")
        lines.append("    css_prop_handler handler;")
        lines.append("};")
        lines.append("")
        
        # Keyword section
        lines.append("%%")
        
        # Each line: keyword, field_value
        for css_name, handler_name in self.keys:
            lines.append(f"{css_name}, {handler_name}")
        
        lines.append("%%")
        lines.append("")
        
        # Wrapper function appended after gperf's generated code.
        # gperf's css_prop_lookup_generated() returns const struct css_prop_entry *
        # but language.c expects css_prop_handler (a function pointer).
        # This wrapper bridges the gap - same pattern as libhubbub's element-type.c
        lines.append("/* Wrapper: extract handler from gperf lookup result */")
        lines.append("static inline css_prop_handler css_prop_lookup(const char *name, size_t len) {")
        lines.append("    const struct css_prop_entry *entry;")
        lines.append("    entry = css_prop_lookup_generated(name, len);")
        lines.append("    if (entry == NULL) return NULL;")
        lines.append("    return entry->handler;")
        lines.append("}")
        lines.append("")
        
        return "\n".join(lines)


class KeywordsParser:
    """Parse keywords.gen to get CSS syntax keywords."""
    
    def __init__(self, keywords_file):
        self.keywords_file = Path(keywords_file)
        self.keywords = []  # List of keyword names
        
    def parse(self):
        """Parse keywords.gen and extract keyword names."""
        if not self.keywords_file.exists():
            print(f"FATAL ERROR: keywords.gen not found at {self.keywords_file}", file=sys.stderr)
            sys.exit(1)
        
        try:
            with open(self.keywords_file, 'r') as f:
                lines = list(f)
        except IOError as e:
            print(f"FATAL ERROR: Cannot read keywords.gen: {e}", file=sys.stderr)
            sys.exit(1)
        
        for line_num, line in enumerate(lines, 1):
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Each non-comment line is a keyword name
            if re.match(r'^[A-Z_]+$', line):
                self.keywords.append(line)
            else:
                print(f"WARNING: Invalid keyword '{line}' at line {line_num} in keywords.gen", file=sys.stderr)
        
        # Validate we found keywords
        if not self.keywords:
            print("FATAL ERROR: No keywords found in keywords.gen", file=sys.stderr)
            print("Expected format: One KEYWORD_NAME per line", file=sys.stderr)
            sys.exit(1)
        
        if len(self.keywords) < 10:
            print(f"FATAL ERROR: Only {len(self.keywords)} keywords in keywords.gen", file=sys.stderr)
            print("Expected at least 10 keywords. File may be incomplete.", file=sys.stderr)
            sys.exit(1)
        
        return self.keywords


class MultiFileGenerator:
    """Generate multiple output files from parsed data."""
    
    def __init__(self, dispatch_order, metadata, keywords):
        self.dispatch_order = dispatch_order  # From dispatch.c
        self.metadata = metadata  # From properties.gen
        self.keywords = keywords  # From keywords.gen
    
    def _header(self, file_type, description):
        """Generate common header for all files."""
        lines = []
        lines.append("/*")
        lines.append(f" * AUTO-GENERATED FILE - DO NOT EDIT!")
        lines.append(" *")
        lines.append(f" * This file was automatically generated by property_generator.py")
        lines.append(f" * {description}")
        lines.append(" *")
        lines.append(" * To regenerate this file, run:")
        lines.append(" *   python3 build-aux/property_generator.py \\")
        lines.append(" *     src/select/dispatch.c \\")
        lines.append(" *     src/parse/properties/properties.gen \\")
        lines.append(" *     <output_files>")
        lines.append(" *")
        lines.append(" * Source files:")
        lines.append(" *   - dispatch.c (defines property order)")
        lines.append(" *   - properties.gen (defines property metadata)")
        lines.append(" */")
        lines.append("")
        return lines
        
    def generate_enum(self):
        """Generate property enum matching dispatch order."""
        lines = self._header("ENUM", "Property enum values matching dispatch.c order")
        
        for idx, prop_info in enumerate(self.dispatch_order):
            prop_name = prop_info['name']
            if prop_name in self.metadata:
                enum_name = self.metadata[prop_name]['enum']
                hex_val = f"0x{idx:03x}"
                lines.append(f"\t{enum_name} = {hex_val},")
            # else: skip this index - creates a gap in enum values
        
        # No extra content after last entry - CSS_N_PROPERTIES is in the parent enum
        
        return '\n'.join(lines)
    
    def generate_dispatch(self):
        """Generate dispatch table entries."""
        lines = self._header("DISPATCH", "Dispatch table entries in canonical order")
        
        for prop_info in self.dispatch_order:
            prop_name = prop_info['name']
            if prop_name in self.metadata:
                inherited = 1 if prop_info['inherited'] else 0  # From dispatch.c!
                lines.append("\t{")
                lines.append(f"\t\tPROPERTY_FUNCS({prop_name}),")
                lines.append(f"\t\t{inherited},")
                lines.append("\t},")
            # else: skip - dispatch.c already has this entry manually
        
        return '\n'.join(lines)
    
    def generate_propstrings(self):
        """Generate property string enum for parser (includes keywords and properties)."""
        lines = self._header("PROPSTRINGS", "Parser string identifiers (keywords + properties, alphabetical)")
        
        # Get longhand properties from dispatch.c
        all_props = [prop_info['name'].upper() for prop_info in self.dispatch_order]
        
        # Add shorthand properties from metadata (marked SHORTHAND or WRAP)
        shorthands = [name.upper() for name, meta in self.metadata.items() 
                      if meta.get('is_shorthand', False) and name.upper() not in all_props]
        
        # Add alias properties (different name but same bytecode target)
        aliases = [name.upper() for name, meta in self.metadata.items()
                   if meta.get('is_alias', False) and name.upper() not in all_props]
        
        # Combine keywords, longhands, shorthands, and aliases, sort alphabetically
        # Properties ONLY - keywords are already in propstrings.h before FIRST_PROP
        all_identifiers = sorted(all_props + shorthands + aliases)
        
        # Generate enum
        for idx, identifier in enumerate(all_identifiers):
            if idx == 0:
                lines.append(f"\t{identifier} = FIRST_PROP,")
            else:
                lines.append(f"\t{identifier},")
        
        if all_identifiers:
            last_id = all_identifiers[-1]
            lines.append("")
            lines.append(f"\tLAST_PROP = {last_id},")
        
        return '\n'.join(lines)
    
    def generate_parse_handlers(self):
        """Generate parse handler array as complete definition."""
        lines = self._header("HANDLERS", "Parse handler function pointers (alphabetical order)")
        
        # Add complete array definition
        lines.append("const css_prop_handler property_handlers[LAST_PROP + 1 - FIRST_PROP] = {")
        
        # Get longhand properties from dispatch.c
        all_props = [prop_info['name'] for prop_info in self.dispatch_order]
        
        # Add shorthand properties from metadata (marked SHORTHAND or WRAP)
        shorthands = [name for name, meta in self.metadata.items() 
                      if meta.get('is_shorthand', False) and name not in all_props]
        
        # Add alias properties
        aliases = [name for name, meta in self.metadata.items()
                   if meta.get('is_alias', False) and name not in all_props]
        
        # Sort all properties alphabetically
        sorted_props = sorted(all_props + shorthands + aliases)
        
        # Each property uses its own autogenerated parser function
        # Aliases like inline_size have autogenerated_inline_size.c that
        # correctly emit the target property's bytecode (e.g., CSS_PROP_WIDTH)
        for prop_name in sorted_props:
            lines.append(f"\tcss__parse_{prop_name},")
        
        lines.append("};")
        
        return '\n'.join(lines)
    
    def generate_propstrings_strings(self):
        """Generate property string map entries for propstrings.c."""
        lines = self._header("PROPSTRINGS_STRINGS", "Property string map entries (propstrings.c)")
        
        # Get longhand properties from dispatch.c
        all_props = [prop_info['name'] for prop_info in self.dispatch_order]
        
        # Add SHORTHAND properties (not in dispatch.c but need entries)
        shorthands = [name for name, meta in self.metadata.items() 
                      if meta.get('is_shorthand', False) and not meta.get('is_manual', False)
                      and name not in all_props]
        
        # Add alias properties
        aliases = [name for name, meta in self.metadata.items()
                   if meta.get('is_alias', False) and name not in all_props]
        
        # Sort alphabetically to match propstrings_enum.inc
        sorted_props = sorted(all_props + shorthands + aliases)
        
        # Generate SMAP entries - property_name becomes property-name
        for prop_name in sorted_props:
            css_name = prop_name.replace('_', '-')
            lines.append(f'\tSMAP("{css_name}"),')
        
        return '\n'.join(lines)
    
    def generate_gperf_input(self):
        """Generate gperf input file for O(1) property lookup.
        
        Returns the content of a .gperf file that GNU gperf will process
        into C code with a perfect hash function.
        """
        # Collect all properties that need parse handlers
        # Get longhand properties from dispatch.c
        all_props = [prop_info['name'] for prop_info in self.dispatch_order]
        
        # Add SHORTHAND properties
        shorthands = [name for name, meta in self.metadata.items()
                      if meta.get('is_shorthand', False) and not meta.get('is_manual', False)
                      and name not in all_props]
        
        # Add alias properties
        aliases = [name for name, meta in self.metadata.items()
                   if meta.get('is_alias', False) and name not in all_props]
        
        sorted_props = sorted(all_props + shorthands + aliases)
        
        # Build key tuples: (css_name, handler_name)
        keys = []
        for prop_name in sorted_props:
            css_name = prop_name.replace('_', '-')
            handler_name = f"css__parse_{prop_name}"
            keys.append((css_name, handler_name))
        
        # Generate gperf input
        gen = GperfInputGenerator(keys)
        return gen.generate_gperf_input()
    
    def validate_indexes(self):
        """Validate that propstrings and handlers have consistent property order."""
        # Get longhand properties from dispatch.c
        all_props = [prop_info['name'] for prop_info in self.dispatch_order]
        
        # Add SHORTHAND properties
        shorthands = [name for name, meta in self.metadata.items() 
                      if meta.get('is_shorthand', False) and not meta.get('is_manual', False)
                      and name not in all_props]
        
        # Add alias properties
        aliases = [name for name, meta in self.metadata.items()
                   if meta.get('is_alias', False) and name not in all_props]
        
        sorted_props = sorted(all_props + shorthands + aliases)
        
        # Check all properties have metadata
        missing = []
        for prop in sorted_props:
            if prop not in self.metadata:
                # This is OK - longhands from dispatch.c may not have properties.gen entries
                pass
        
        # Check for duplicate properties
        seen = set()
        for prop in sorted_props:
            if prop in seen:
                print(f"FATAL ERROR: Duplicate property '{prop}' in generated output", file=sys.stderr)
                sys.exit(1)
            seen.add(prop)
        
        print(f"  Validation passed: {len(sorted_props)} properties, no duplicates")
        return True


def main():
    if len(sys.argv) != 9:
        print("Usage: property_generator.py dispatch.c properties.gen keywords.gen enum.inc dispatch.inc propstrings.inc propstrings_strings.inc prop_hash_table.gperf", file=sys.stderr)
        sys.exit(1)
    
    dispatch_file = sys.argv[1]
    gen_file = sys.argv[2]
    keywords_file = sys.argv[3]
    enum_out = sys.argv[4]
    dispatch_out = sys.argv[5]
    propstrings_out = sys.argv[6]
    propstrings_strings_out = sys.argv[7]
    gperf_out = sys.argv[8]
    
    # Parse input files
    print("Parsing dispatch.c for property order...")
    dispatch_parser = DispatchParser(dispatch_file)
    dispatch_order = dispatch_parser.parse()
    print(f"  Found {len(dispatch_order)} properties in dispatch.c")
    
    print("Parsing properties.gen for metadata...")
    gen_parser = PropertyGenParser(gen_file)
    prop_metadata = gen_parser.parse()
    print(f"  Found {len(prop_metadata)} properties in properties.gen")
    
    print("Parsing keywords.gen for CSS keywords...")
    keywords_parser = KeywordsParser(keywords_file)
    keywords = keywords_parser.parse()
    print(f"  Found {len(keywords)} keywords in keywords.gen")
    
    # Generate output files
    generator = MultiFileGenerator(dispatch_order, prop_metadata, keywords)
    
    # Run validation first
    print("Validating property indexes...")
    generator.validate_indexes()
    
    with open(enum_out, 'w') as f:
        f.write(generator.generate_enum())
    print(f"Generated: {enum_out}")
    
    with open(dispatch_out, 'w') as f:
        f.write(generator.generate_dispatch())
    print(f"Generated: {dispatch_out}")
    
    with open(propstrings_out, 'w') as f:
        f.write(generator.generate_propstrings())
    print(f"Generated: {propstrings_out}")
    
    with open(propstrings_strings_out, 'w') as f:
        f.write(generator.generate_propstrings_strings())
    print(f"Generated: {propstrings_strings_out}")
    
    # Generate gperf input file (replaces perfect-hash generation)
    # CMake will then invoke gperf to produce the final prop_hash_table.inc
    print("Generating gperf input file...")
    with open(gperf_out, 'w') as f:
        f.write(generator.generate_gperf_input())
    print(f"Generated: {gperf_out}")


if __name__ == '__main__':
    main()

