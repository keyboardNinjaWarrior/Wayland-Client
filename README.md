# Qurtuba

A low-level Wayland client abstraction layer written in C. Qurtuba simplifies communication with Wayland compositors by providing clean wrappers around the complex `wayland-client.h` API, enabling developers to build graphics applications for Wayland without wrestling with protocol boilerplate.

## What this is

Qurtuba is a minimal but functional Wayland client framework that handles the core initialization, surface management, and event dispatching required to render to a Wayland compositor. The project demonstrates how to set up a windowed application on Wayland from scratch, managing buffers, handling configuration callbacks, and coordinating rendering across multiple queues. It includes a simple demo that draws a white circle on a black background.

### Stack

- **Language:** C (C11 with GNU extensions)
- **Runtime:** Native binary linking against `libwayland-client`
- **Key dependencies:**
  - `libwayland-client` — the core Wayland protocol library
  - `wayland-protocols` — provides XDG shell protocol definitions
  - Standard POSIX libraries (`pthreads`, `mman`, `sys/mman` for shared memory)

## How it's organized

```
.
├── include/          Header files
│   ├── render.h       Pixel buffer drawing interface
│   ├── errors.h       Bitfield error codes for Wayland objects
│   └── debug.h         Debugging macros (colors, logging, benchmarking)
├── src/              Implementation
│   ├── main.c          Wayland client lifecycle, event loop, queue dispatch
│   └── render.c        Circle rendering demo (pixel buffer manipulation)
└── makefile          Build rules and entry points
```

Generated at build time (not committed):

- `lib/xdg-shell-protocol.c` — compiled from wayland-protocols
- `include/xdg-shell-client-protocol.h` — protocol headers
- `object/*.o` — compiled object files
- `bin/main`, `bin/main-debug` — final executables

**How it fits together:**

The application starts in `main()` by connecting to the Wayland display, creating a registry listener to bind to the compositor, shared memory allocator, and window manager. It creates an XDG toplevel surface (a standard window) and sets up two event dispatch threads: one for display events (configuration, ping) and one for render callbacks. When the compositor signals that a frame is ready via the callback listener, the render thread calls `draw()` to populate the pixel buffer, then commits it. A double-buffering scheme using two frame buffers ensures smooth rendering without tearing. On close, all Wayland objects are destroyed, memory is unmapped, and the application exits cleanly.

## How to run it

### Prerequisites

- GCC or Clang
- `libwayland-client-dev` (or equivalent dev package for your distro)
- `wayland-protocols` package
- `wayland-scanner` tool
- POSIX threads support
- A running Wayland session (not X11)

### Build and Run

```bash
# Build the main executable
make build

# Run the application
make run

# Build with debugging enabled (includes color logging and benchmarking)
make build-debug

# Run the debug build
make run-debug

# Profile with gprof (requires debug build)
make profiler

# Debug with gdb
make debugger

# Clean up object files and binaries
make remove
```
