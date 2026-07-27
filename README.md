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
- [Usage](#usage)
- [API reference](#api-reference)
- [Contributing](#contributing)
- [Author](#author)

## Stack

- **Language:** C
- **Framework / runtime:** Wayland protocol (stable XDG shell)
- **Notable libraries:** wayland-client, POSIX threads (`<threads.h>`), memory mapping (`mmap`)

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
- `wayland-client` development libraries
- `wayland-protocols` package (for XDG shell XML, typically at `/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml`)
- `wayland-scanner` tool
- A running Wayland compositor (e.g., Sway, Weston, GNOME on Wayland) to test against

### Build

```bash
# Build the library object file
make build

# Clean object files
make clean
```

The build output is `./object/qurtuba.o`, which can be linked into your own client application.

## Usage

```c
#include "src/qurtuba.h"

// Create a 1024×768 window titled "My App"
struct state *window = qurtuba_create_window("My App", 1024, 768);
```

Link `qurtuba.o` alongside your application code and `-lwayland-client` when building.

## API reference

### `qurtuba_create_window`

```c
struct state *qurtuba_create_window(const char *title, int width, int height);
```

Creates a Wayland window with an XDG toplevel surface and initializes double-buffered rendering.

| Parameter | Type | Description |
|---|---|---|
| `title` | `const char *` | Window title shown by the compositor |
| `width` | `int` | Initial window width in pixels |
| `height` | `int` | Initial window height in pixels |

**Returns:** a pointer to a `struct state` representing the window/client state, or `NULL` on failure (check `errors.h` for error codes).

## Contributing

Issues and pull requests are welcome. Please keep changes focused and include a brief description of what problem they solve.

## Author

SilentSoul8R

<!-- Add your license here, e.g. MIT, Apache-2.0 -->
