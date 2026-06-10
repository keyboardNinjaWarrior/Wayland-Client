#ifndef DEBUG_H
#define	DEBUG_H

#define ERR_DISPLAY		0b000000001
#define ERR_COMPOSITOR		0b000000010
#define ERR_SHM			0b000000100
#define ERR_XDG_WM_BASE		0b000001000
#define ERR_WL_SURFACE		0b000010000
#define ERR_XDG_SURFACE 	0b000100000
#define ERR_XDG_TOPLEVEL	0b001000000
#define ERR_MEM			0b010000000
#define	ERR_FILE		0b100000000

#ifdef 	DEBUG
#include <stdio.h>
#define LOG				"\x1B[1m" "[" "\x1B[33m" "LOG" "\x1B[39m" "]" "\x1B[0m"
#define	SUCCESS				"\x1B[1m" "[" "\x1B[34m" "SUCCESS" "\x1B[39m" "]" "\x1B[0m"
#define	FAIL				"\x1B[1m" "[" "\x1B[31m" "FAIL" "\x1B[39m" "]" "\x1B[0m"
#define print_log(type, string, ...) 	fprintf(stderr,  type " " string " (" "\x1B[1;31m" "%s" "\x1B[0m" ":" "\x1B[1m"  "\x1B[38;5;75m" "%s" "\x1B[0m" ":" "\x1B[1;32m" "%d" "\x1B[0m" ")" "\n", ##__VA_ARGS__, __FILE__, __func__, __LINE__);
#else
#define print_log(...)
#endif

#endif

