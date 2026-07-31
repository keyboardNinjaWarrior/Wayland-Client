<!--Author: Sanghera -->

<div align="center">
  
  <img width="250" height="250" alt="Logo_Qurtuba__1_-removebg-preview (1)" src="https://github.com/user-attachments/assets/7a6b8d71-cc58-4775-b668-c9bc4246373c" />
</div>







# Qurtuba

A lightweight C library that provides an abstraction layer over `wayland-client.h`, enabling developers to create Wayland windows and render pixel buffers with minimal boilerplate.

## Features

- Simple, single-call window creation (`qurtuba_create_window`)
- Handles compositor, shared-memory, and window-manager binding for you
- Double-buffered rendering with shared memory pools, no manual buffer juggling
- Separate event-dispatch threads for lifecycle events and render callbacks, so rendering never blocks input handling
- Built-in bitfield-based error codes and optional colored debug logging

## Table of contents

- [Stack](#stack)
- [How it's organized](#how-its-organized)
- [How it fits together](#how-it-fits-together)
- [Installation](#installation)
- [Contributing](#contributing)

## Stack

- **Language:** C
- **Runtime:** Wayland protocol (stable XDG shell)

## How it's organized

```
src/
├── qurtuba.c    Core Wayland client implementation, window creation, buffer management, and event dispatching
├── qurtuba.h    Public API header (qurtuba_create_window)
└── render.c     Pixel drawing logic (currently draws circles)

include/
├── errors.h     Error code definitions (bitfield-based error codes)
├── render.h     Render function interface
└── debug.h      Debug logging and benchmarking macros (with colors)

makefile         Build rules: compiles object files, generates XDG shell protocol from Wayland XML, links into qurtuba.o
```

## How it fits together

Qurtuba initializes a Wayland display connection, binds to the compositor, shared memory, and window manager interfaces, then creates an XDG toplevel surface. It manages two frame buffers using shared memory pools and dispatches Wayland events on separate threads, one for configuration/lifecycle events (default queue) and one for render callbacks (render queue). The render loop waits for buffer-release events, then invokes the `draw()` function to write pixel data and commits the surface.

## Installation

### Prerequisites

- A C compiler (GCC or Clang)
- `wayland-devel` development package for wayland which includes:
    - `wayland-client` development libraries
    - `wayland-protocols` package (for XDG shell XML, typically at `/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml`)
    - `wayland-scanner` tool for converting XML protocol files into code
- A running Wayland compositor (e.g., Sway, Weston, GNOME on Wayland) to test against

### Build

To build the object file:
```bash
make build
```
To clean the object folder:
```bash
make clean
```

The build output is `./object/qurtuba.o`, which can be linked into your own client application.

## Contributing

Issues and pull requests are welcome. Please keep changes focused and include a brief description of what problem they solve.

---
<div align = center>
    <img width="250" height="250" alt="Logo Qurtuba" src="https://github.com/user-attachments/assets/ec8e866b-f889-4768-a008-5643491cea84"  />
</div>

