#!/usr/bin/env python3
import re
import os
from collections import defaultdict

SOURCE_EXT = {'.cpp', '.hpp', '.h'}
DEFINE_CLASS_RE = re.compile(r'\bclass\s+([A-Za-z_]\w+)')
# Rough function definition regex: excludes control statements
FUNC_DEF_RE = re.compile(r'^(?!\s*(if|for|while|switch|return)\b)[A-Za-z_]\w*[\*&\s]+([A-Za-z_]\w+)\s*\([^;]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->[^{]*)?\{')
# Method definition with scope
METHOD_DEF_RE = re.compile(r'^(?:template\s*<[^>]+>\s*)?(?:[A-Za-z_]\w*[\*&\s]+)?([A-Za-z_]\w+)::([A-Za-z_]\w+)\s*\([^;]*\)')

SYMBOL_WORD_RE = re.compile(r'\b([A-Za-z_]\w+)\b')
CALL_SCOPE_RE = re.compile(r'\b([A-Za-z_]\w+)::([A-Za-z_]\w+)\s*\(')

class Symbol:
    def __init__(self, name, kind, file, line):
        self.name = name
        self.kind = kind  # class|function|method
        self.file = file
        self.line = line
        self.usages = 0
        self.definition_count = 1
        # For methods, store base parts if applicable
        if kind == 'method' and '::' in name:
            parts = name.split('::', 1)
            self.class_name = parts[0]
            self.method_name = parts[1]
        else:
            self.class_name = None
            self.method_name = None
    def loc(self):
        return f"{self.file}:{self.line}"


def walk_files(root):
    for dirpath, _, filenames in os.walk(root):
        for f in filenames:
            ext = os.path.splitext(f)[1]
            if ext in SOURCE_EXT:
                yield os.path.join(dirpath, f)


def collect_symbols(root):
    symbols = {}
    # map name to symbol (first definition recorded)
    for path in walk_files(root):
        try:
            with open(path, 'r', errors='ignore') as fh:
                for ln, line in enumerate(fh, 1):
                    clm = DEFINE_CLASS_RE.search(line)
                    if clm and '{' in line:  # likely definition not forward decl
                        name = clm.group(1)
                        if name not in symbols:
                            symbols[name] = Symbol(name, 'class', path, ln)
                    mscope = METHOD_DEF_RE.search(line)
                    if mscope and '{' in line:
                        cname, mname = mscope.group(1), mscope.group(2)
                        full = f"{cname}::{mname}"
                        if full not in symbols:
                            symbols[full] = Symbol(full, 'method', path, ln)
                    fdef = FUNC_DEF_RE.search(line)
                    if fdef:
                        fname = fdef.group(2)
                        if fname not in symbols:
                            symbols[fname] = Symbol(fname, 'function', path, ln)
        except Exception as e:
            print(f"WARN: failed reading {path}: {e}")
    return symbols


def count_usages(root, symbols):
    # Build mapping name->symbol for quick check
    name_map = defaultdict(list)
    short_method_map = defaultdict(list)
    for sym in symbols.values():
        name_map[sym.name].append(sym)
        if sym.kind == 'method' and sym.method_name:
            short_method_map[sym.method_name].append(sym)
    for path in walk_files(root):
        try:
            with open(path, 'r', errors='ignore') as fh:
                for ln, line in enumerate(fh, 1):
                    # Skip comment-only lines for noise reduction
                    stripped = line.strip()
                    if stripped.startswith('//') or stripped.startswith('/*'):
                        continue
                    # Count qualified calls like Class::Method(
                    for cls, mth in CALL_SCOPE_RE.findall(line):
                        full = f"{cls}::{mth}"
                        if full in name_map:
                            for sym in name_map[full]:
                                if not (sym.file == path and sym.line == ln):
                                    sym.usages += 1
                    for word in SYMBOL_WORD_RE.findall(line):
                        if word in name_map:
                            for sym in name_map[word]:
                                # don't count the definition line itself as usage
                                if not (sym.file == path and sym.line == ln):
                                    sym.usages += 1
                        # Also count unqualified method name occurrences for methods
                        if word in short_method_map:
                            for sym in short_method_map[word]:
                                if not (sym.file == path and sym.line == ln):
                                    sym.usages += 1
        except Exception as e:
            print(f"WARN: failed reading {path}: {e}")


def main():
    import argparse
    ap = argparse.ArgumentParser(description='Heuristic unused symbol analyzer (classes, functions, methods).')
    ap.add_argument('--root', default='.', help='Root directory of source to analyze')
    ap.add_argument('--min-usages', type=int, default=0, help='Threshold: symbols with usages <= this reported')
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    symbols = collect_symbols(root)
    count_usages(root, symbols)
    # Treat constructors cautiously: if a method name equals its class name,
    # skip reporting as unused (constructor usages are not easily detected).
    def is_constructor(sym):
        return sym.kind == 'method' and sym.method_name == sym.class_name

    unused = [s for s in symbols.values() if (s.usages <= args.min_usages and not is_constructor(s))]
    # Sort: kind then name
    unused.sort(key=lambda s: (s.kind, s.name.lower()))
    print(f"Total symbols detected: {len(symbols)}")
    print(f"Symbols with usages <= {args.min_usages}: {len(unused)}")
    print("Kind,Name,DefLocation,Usages")
    for s in unused:
        print(f"{s.kind},{s.name},{s.loc()},{s.usages}")
    print('\nNOTE: Heuristic only. False positives likely: inline templates, macro uses, indirect calls, virtual dispatch, symbol references via pointers, assembly, generated code.')

if __name__ == '__main__':
    main()
