# Tools

This folder contains utility scripts used during development.

## analyze_unused.py
A heuristic analyzer to find potentially unused C++ symbols (classes, free functions, and methods).

### Usage
Run against the source tree you want to analyze. For the kernel sources:

```bash
python3 tools/analyze_unused.py --root kernel/src
```

Or analyze the whole repo:

```bash
python3 tools/analyze_unused.py --root .
```

Optional threshold (default 0) to include rarely used symbols:

```bash
python3 tools/analyze_unused.py --root kernel/src --min-usages 1
```

### Notes
- This is a heuristic; it may report false positives for templates, macros, virtual dispatch, indirect calls, generated code, etc.
- It only scans files with extensions `.cpp`, `.hpp`, `.h`.

### Environment
- Requires Python 3.8+ (no external packages).
- On Linux:

```bash
python3 --version
python3 tools/analyze_unused.py --root kernel/src
```
