#ifndef ERRORS_H
#define ERRORS_H

#define ERR_DISPLAY		(0b0001 << 0)
#define ERR_COMPOSITOR		(0b0010 << 0)
#define ERR_SHM			(0b0100 << 0)
#define ERR_XDG_WM_BASE		(0b1000 << 0)
#define ERR_WL_SURFACE		(0b0001 << 4)
#define ERR_XDG_SURFACE 	(0b0010 << 4)
#define ERR_XDG_TOPLEVEL	(0b0100 << 4)
#define ERR_MEM			(0b1000 << 4)
#define	ERR_FILE		(0b0001 << 8)

#endif
