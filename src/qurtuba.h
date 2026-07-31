#ifndef QURTUBA_H
#define QURTUBA_H

#include <stdint.h>

typedef struct state * qurtuba_window;

struct state * qurtuba_create_window(char * title, uint16_t width, uint16_t height, void (* draw) (uint32_t *, uint16_t, uint16_t));
void qurtuba_launch_window(qurtuba_window window);
void qurtuba_close_window(qurtuba_window window);

#endif 
