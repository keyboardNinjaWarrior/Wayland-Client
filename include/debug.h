#ifndef DEBUG_H
#define	DEBUG_H

	
#ifdef 	DEBUG
	#include <stdio.h>
	#include <time.h>			

	#define	BOLD		"\x1B[1m"
	#define COLOR(r, g, b)	"\x1B[38;2;" #r ";" #g ";" #b "m"
	#define RESET		"\x1B[0m"
	#define DEFAULT_COLOR	"\x1B[39m"

	#define LOG		BOLD "[" COLOR(245, 194, 17) "LOG" DEFAULT_COLOR "]" RESET
	#define	SUCCESS		BOLD "[" COLOR(17, 188, 240) "SUCCESS" DEFAULT_COLOR "]" RESET
	#define	FAIL		BOLD "[" COLOR(225, 51, 51) "FAIL" DEFAULT_COLOR "]" RESET
	#define BENCHMARK	BOLD "[" COLOR(249, 240, 107) "BENCHMARK" DEFAULT_COLOR "]" RESET

	// > warning:  backslash  and  newline  separated  by 
	// space. This warning is  result of missing or extra 
	// backlashes.
	 
	#define PRINT_LOG(type, string, ...)	{									\
							fprintf(stderr, 						\
								BOLD COLOR(246, 130, 255) "%s" RESET ":" 		\
								BOLD COLOR(255, 120, 0) "%s" RESET ":" 			\
								BOLD COLOR(0, 204, 156) "%d" RESET "\n"			\
								type " " string "\n\n", 				\
								__FILE__, __func__, __LINE__, ##__VA_ARGS__);		\
						}

	// XXX: If you declare a variable inside START_BENCH-
	// MARK and END_BENCHMARK,  you  can't use it outside 
	// it
	
	// n  is a number. It identifies the starting  bench-
	// mark. Hence, in a given  scope n should be unique. 
	// For example:
	// 		
	//	```
	//	START_BENCHMARK(1)
	//	...
	//	START_BENCHMARK(1)
	//	```
	//
	// is not allowed. However,  for every unique n there 
	// can be multiple END_BENCHMARKS(n)

	#define START_BENCHMARK(n)		clock_t start_ ## n = clock();

						/* some function */

	#define END_BENCHMARK(n, string, ...)	{										\
							clock_t end_ ## n = clock();						\
																\
							PRINT_LOG(BENCHMARK, 							\
								  "\u0394time = " BOLD "%f sec" RESET ": "			\
								  string,							\
								  (((float) ((end_ ## n) - (start_ ## n))) / CLOCKS_PER_SEC),	\
								  ##__VA_ARGS__);						\
						}
#else
	#define PRINT_LOG(...)

	#define START_BENCHMARK(...)
	#define END_BENCHMARK(...)
#endif
#endif
