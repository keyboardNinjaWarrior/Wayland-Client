#ifndef QURTUBA_H
#define QURTUBA_H

#include <stdint.h>

typedef struct state * qurtuba_window;

struct state * qurtuba_create_window(char * title, uint16_t width, uint16_t height);

#endif 
