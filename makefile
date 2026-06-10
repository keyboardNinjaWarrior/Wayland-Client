XDG_SHELL := /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml 
DYNAMIC_C_LIB := -lc
DYNAMIC_LIB := -lwayland-client $(DYNAMIC_C_LIB)

.PHONY: run, remove, debug

main: main.o render.o xdg-shell-protocol.o
	@echo "[cc] Compiling into executable: main..."
	@cc -o main main.o render.o xdg-shell-protocol.o $(DYNAMIC_LIB)
	@echo "Running..."
	@./main

debug: main.o render.o xdg-shell-protocol.o
	@echo "[cc] Compiling into executable with debug header: main..."
	@cc -DDEBUG -o main main.c render.c xdg-shell-protocol.c $(DYNAMIC_LIB)
	@echo "Running..."
	@./main

main.o: main.c render.h
	@echo "[cc] Compiling object file: main.o..."
	@cc -o main.o -c main.c

render.o: render.c render.h
	@echo "[cc] Compiling object file: render.o..."
	@cc -o render.o -c render.c

xdg-shell-protocol.o: xdg-shell-protocol.c
	@echo "[cc] Compiling object file: xdg-shell-protocol.o..."
	@cc -o xdg-shell-protocol.o -c xdg-shell-protocol.c 

xdg-shell-protocol.c: $(XDG_SHELL)
	@echo "[wayland-scanner] Generating xdg-shell-protocol.c..."
	@wayland-scanner private-code $(XDG_SHELL) xdg-shell-protocol.c

xdg-shell-client-protocol.h: $(XDG_SHELL)
	@echo "[wayland-scanner] Generating xdg-shell-protocol.h..."
	@wayland-scanner client-header $(XDG_SHELL) xdg-shell-client-protocol.h

run:
	@./main

remove:
	@rm main *.o
