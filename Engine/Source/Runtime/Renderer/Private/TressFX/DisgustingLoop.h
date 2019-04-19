#pragma once

#if(LOOP_END > LOOP_MAX) 
#error Loop Counter Too Big
#endif

MACRO(LOOP_START)

#if(LOOP_START == 0)
#undef LOOP_START
#define LOOP_START 1
#elif(LOOP_START == 1)
#undef LOOP_START 
#define LOOP_START 2
#elif(LOOP_START == 2)
#undef LOOP_START 
#define LOOP_START 3
#elif(LOOP_START == 3)
#undef LOOP_START 
#define LOOP_START 4
#elif(LOOP_START == 4)
#undef LOOP_START 
#define LOOP_START 5
#elif(LOOP_START == 5)
#undef LOOP_START 
#define LOOP_START 6
#elif(LOOP_START == 6)
#undef LOOP_START 
#define LOOP_START 7
#elif(LOOP_START == 7)
#undef LOOP_START 
#define LOOP_START 8
#elif(LOOP_START == 8)
#undef LOOP_START 
#define LOOP_START 9
#elif(LOOP_START == 9)
#undef LOOP_START 
#define LOOP_START 10
#elif(LOOP_START == 10)
#undef LOOP_START 
#define LOOP_START 11
#elif(LOOP_START == 11)
#undef LOOP_START 
#define LOOP_START 12
#elif(LOOP_START == 12)
#undef LOOP_START 
#define LOOP_START 13
#elif(LOOP_START == 13)
#undef LOOP_START 
#define LOOP_START 14
#elif(LOOP_START == 14)
#undef LOOP_START 
#define LOOP_START 15
#elif(LOOP_START == 15)
#undef LOOP_START 
#define LOOP_START 16
#elif(LOOP_START == 16)
#undef LOOP_START 
#define LOOP_START 17
#elif(LOOP_START == 17)
#undef LOOP_START 
#define LOOP_START 18
#elif(LOOP_START == 18)
#undef LOOP_START 
#define LOOP_START 19
#elif(LOOP_START == 19)
#undef LOOP_START 
#define LOOP_START 20
#elif(LOOP_START == 20)
#undef LOOP_START 
#define LOOP_START 21
#elif(LOOP_START == 20)
#undef LOOP_START 
#define LOOP_START 22
#elif(LOOP_START == 21)
#undef LOOP_START 
#define LOOP_START 23
#elif(LOOP_START == 22)
#undef LOOP_START 
#define LOOP_START 24
#elif(LOOP_START == 23)
#undef LOOP_START 
#define LOOP_START 24
#elif(LOOP_START == 24)
#undef LOOP_START 
#define LOOP_START 25
#elif(LOOP_START == 25)
#undef LOOP_START 
#define LOOP_START 26
#elif(LOOP_START == 26)
#undef LOOP_START 
#define LOOP_START 27
#elif(LOOP_START == 27)
#undef LOOP_START 
#define LOOP_START 28
#elif(LOOP_START == 28)
#undef LOOP_START 
#define LOOP_START 29
#elif(LOOP_START == 29)
#undef LOOP_START 
#define LOOP_START 30
#elif(LOOP_START == 30)
#undef LOOP_START 
#define LOOP_START 31
#elif(LOOP_START == 31)
#undef LOOP_START 
#define LOOP_START 32
#elif(LOOP_START == 32)
#undef LOOP_START 
#define LOOP_START 33
#endif


#if(LOOP_START < LOOP_END)
#include "DisgustingLoopStart.h"
#endif