#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>

void printhelp();

#endif //UTILS_H


// TODO: reset LOG definition
#ifdef DEBUG
	#include <stdio.h>
	#define LOG(x) fprintf(stderr, "%s\n", x);
	//#define LOG(x) fprintf(stderr,x);
#else
	#define LOG(x) ;
#endif
