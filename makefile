XDG_SHELL := /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml 
DYNAMIC_LIB := -lwayland-client -lc
INCLUDE := -I./include/

.PHONY: run, remove, run-debug, build, build-debug, debugger, profiler

# Simple Build
build: ./bin/main

./bin/main: ./object/main.o ./object/render.o ./object/xdg-shell-protocol.o bin
	@echo "[cc] Linking files into executable: main..."
	@cc -o ./bin/main ./object/main.o ./object/render.o ./object/xdg-shell-protocol.o $(DYNAMIC_LIB)

./object/main.o: ./src/main.c ./include/render.h ./include/errors.h ./include/xdg-shell-client-protocol.h object
	@echo "[cc] Compiling object file: main.o..."
	@cc $(INCLUDE) -o ./object/main.o -c ./src/main.c

./object/render.o: ./src/render.c ./include/render.h object
	@echo "[cc] Compiling object file: render.o..."
	@cc $(INCLUDE) -o ./object/render.o -c ./src/render.c

# Build with debugging
build-debug: ./bin/main-debug

./bin/main-debug: ./object/render-debug.o ./object/xdg-shell-protocol.o ./object/main-debug.o bin
	@echo "[cc] Linking files into executable main with debug header..."
	@cc -pg -g3 -o ./bin/main-debug ./object/render-debug.o ./object/xdg-shell-protocol.o ./object/main-debug.o $(DYNAMIC_LIB)

./object/main-debug.o: ./src/main.c ./include/render.h ./include/debug.h ./include/errors.h ./include/xdg-shell-client-protocol.h object
	@echo "[cc] Compiling object file: main.o with debug header..."
	@cc $(INCLUDE) -pg -g3 -DDEBUG -o ./object/main-debug.o -c ./src/main.c

./object/render-debug.o: ./src/render.c ./include/errors.h object
	@echo "[cc] Compiling object file: render.o with debug header..."
	@cc $(INCLUDE) -pg -g3 -DDEBUG -o ./object/render-debug.o -c ./src/render.c

# Protocols generated from computer
./lib/xdg-shell-protocol.c: $(XDG_SHELL) lib
	@echo "[wayland-scanner] Generating xdg-shell-protocol.c..."
	@wayland-scanner private-code $(XDG_SHELL) ./lib/xdg-shell-protocol.c

./include/xdg-shell-client-protocol.h: $(XDG_SHELL) include
	@echo "[wayland-scanner] Generating xdg-shell-protocol.h..."
	@wayland-scanner client-header $(XDG_SHELL) ./include/xdg-shell-client-protocol.h

./object/xdg-shell-protocol.o: ./lib/xdg-shell-protocol.c object
	@echo "[cc] Compiling object file: xdg-shell-protocol.o..."
	@cc -o ./object/xdg-shell-protocol.o -c ./lib/xdg-shell-protocol.c 

# Generating directories
bin object lib:
	@echo "[LOG] Generating directories..."
	@mkdir $@ 

# Magic/Phony Commands
run: ./bin/main
	@echo "Running..."
	@./bin/main

run-debug: ./bin/main-debug
	@echo "Running..."
	@./bin/main-debug

debugger:
	@echo "Running debugger..."
	gdb ./bin/main-debug

remove:
	- @rm ./object/*.o
	- @rm ./bin/*

profiler: ./bin/main-debug
	gprof -b ./bin/main-debug
