#ifndef QURTUBA_H
#define QURTUBA_H

#include <stdint.h>

typedef struct state * qurtuba_window;

qurtuba_window qurtuba_create_window(char * title, uint16_t width, uint16_t height);
void qurtuba_launch_window(qurtuba_window window);
void qurtuba_close_window(qurtuba_window window);

#endif 
