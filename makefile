XDG_SHELL := /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml 
DYNAMIC_LIB := -lwayland-client
INCLUDE := -I./include/
OBJECT ?= ./object
DEBUG_FLAGS := -pg -g3 -DDEBUG

.PHONY: clean build directories

# TODO: Find a way to overide OBJECT variable and set it to argument passed to makefile

# Project Build
build: unlinked-qurtuba.o xdg-shell-protocol.o xdg-shell-protocol.o
	@echo "[ld] Combining object files togather into qurtuba.o: unlinked-qurtuba.o xdg-shell-protocol.o xdg-shell-protocol.o..." 
	@ld -r $(OBJECT)/unlinked-qurtuba.o $(OBJECT)/xdg-shell-protocol.o -o $(OBJECT)/qurtuba.o

unlinked-qurtuba.o: ./src/qurtuba.c ./include/errors.h ./xdg-shell-protocol/xdg-shell-client-protocol.h directories 
	@echo "[cc] Compiling object file: $@"
	@cc $(DEBUG_FLAGS) $(INCLUDE) -I./xdg-shell-protocol -o $(OBJECT)/unlinked-qurtuba.o -c ./src/qurtuba.c $(DYNAMIC_LIB)

# Protocols generated locally 
xdg-shell-protocol.o: ./xdg-shell-protocol/xdg-shell-protocol.c directories 
	@echo "[cc] Compiling object file: $@..."
	@cc -o $(OBJECT)/xdg-shell-protocol.o -c ./xdg-shell-protocol/xdg-shell-protocol.c

xdg-shell-protocol.c: $(XDG_SHELL) directories
	@echo "[wayland-scanner] Generating $@..."
	@wayland-scanner private-code $(XDG_SHELL) ./xdg-shell-protocol/xdg-shell-protocol.c

xdg-shell-client-protocol.h: $(XDG_SHELL) directories  
	@echo "[wayland-scanner] Generating $@..."
	@wayland-scanner client-header $(XDG_SHELL) ./xdg-shell-protocol/xdg-shell-client-protocol.h

# Generating directories
directories: $(OBJECT) ./xdg-shell-protocol 

./xdg-shell-protocol:
	@echo "[LOG] Generating directoty: $@..."
	- @mkdir ./xdg-shell-protocol 

$(OBJECT):
	@echo "[LOG] Generating directory: $@..."
	- @mkdir $(OBJECT)

# Cleaning object files
clean:
	- @rm $(OBJECT)/*.o
