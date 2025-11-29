Boot Stages Overview
====================

This document describes the ordered boot stages introduced to provide a
clear, Ubuntu/Linux-inspired progression during kernel startup. Each stage
is logged via `BOOT STAGE=<StageName>` and advances strictly forward.

Stages
------
1. EarlyInit
   - Multiboot parsing, basic console (TTY clear), descriptor tables (GDT), interrupt manager (IDT) setup.
2. MemoryInit
   - Physical Memory Manager, paging structures, heap allocator, scheduler primitives, pipe/thread managers.
3. DriverInit
   - Core hardware drivers: PCI scan, keyboard, mouse handlers, storage (ATA), network devices, VGA/framebuffer basics.
4. FilesystemInit
   - ATA device probing, FAT16/FAT32 mounts, root filesystem selection, global `g_fs_ptr` populated.
5. ServicesInit
   - Kernel services registered and started (filesystem, banner, time, window manager, init daemon). Mouse poll mode applied.
6. MultitaskingStart
   - Scheduler activated; threads can run; process spawning enabled.
7. GraphicsMode
   - Conditional: only logged if a framebuffer is detected (via `gfx::IsAvailable()`). Window manager already running.
8. ShellInit
   - Graphical threaded shell started (or fallback text shell). User interaction becomes available.
9. Complete
   - Boot sequence finished; system enters steady-state loop (idle + periodic scheduling).

Design Goals
------------
- Deterministic ordering with minimal code churn in existing init functions.
- Easy grepping in logs for performance measurement or regression detection.
- Extensible: new stages can be added without breaking existing ordering (append-only). Avoid re-numbering.
- Lightweight: no dynamic allocation or complex state machine required.

Extending Stages
----------------
To add a new stage, append it to `BootStage` in `include/kernel/boot_stage.hpp`, implement the transition point in `kernel_main.cpp`, and ensure it logs meaningful completion criteria.

Conditional Graphics Stage
--------------------------
The `GraphicsMode` stage is emitted only if a framebuffer was initialized (`gfx::IsAvailable()` returns true). Text-only boots will skip directly from `MultitaskingStart` to `ShellInit`.

Future Work
-----------
- Add command-line flag (e.g., `nogfx`) to suppress window manager and graphical shell even if framebuffer exists.
- Map stages to a more granular target system (akin to `systemd` targets) if service dependencies grow.

Boot Timing & Profiling
-----------------------
The `BootProgressor` now records an optional millisecond timestamp for each stage transition by sampling a monotonic uptime source when available. When the boot sequence reaches the `Complete` stage, it emits a compact timing summary with two deltas per stage:

- Time since boot start (relative to `EarlyInit`).
- Time since the previous recorded stage.

This allows simple profiling by grepping for the timing lines in the kernel log and comparing deltas across boots or revisions.
