#include <stdint.h>
#include "debug.h"

void draw (uint32_t * pixels, uint16_t height, uint16_t width)
{
	print_log(LOG, "Drawing...");

	uint32_t index(uint16_t x, uint16_t y)
	{
		return (y * width) + x;
	}

	uint32_t translate_index(int32_t x, int32_t y)
	{
		// Translation:
		// (x',y') = (x + Δx, y + Δy)
		
		// Reflection:
		// (x',y') = (-x, -y)

		int32_t a = x + (width / 2);
		int32_t b = -y + (height / 2);
		
		// print_log(LOG, "(%d, %d)", a, b);
		
		return index(a, b);
	}
	
	int32_t translate_x(uint16_t x)
	{
		return x - (width / 2); 
	}

	int32_t translate_y(uint16_t y)
	{
		return -(y - (height / 2)); 
	}

	for(uint16_t y = 0; y < height; y++)
	{
		for(uint16_t x = 0; x < width; x++)
		{
			pixels[index(x, y)] = 0xFF000000;
		}
	}

	uint64_t square(int64_t a)
	{
		return a * a;
	}
	
	void draw_circle(void)
	{
		for(uint16_t y = 0; y < height; y++)
		{
			for(uint16_t x = 0; x < width; x++)
			{
				if ( square(translate_x(x)) + square(translate_y(y)) == square(200))
				{
					print_log(LOG, "(%d, %d) -> (%d, %d)", translate_x(x), translate_y(y), x, y);
					pixels[index(x, y)] = 0xFFFFFFFF;
				}
			}
		}
	}

	draw_circle();
}
