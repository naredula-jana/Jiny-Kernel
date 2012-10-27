#ifndef __TYPES_h
#define __TYPES_h

//typedef long unsigned int size_t
typedef long  size_t;
//typedef long  ssize_t;
typedef unsigned long  addr_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

# if __WORDSIZE == 64
typedef long int                int64_t;
# else
__extension__
typedef long long int           int64_t;
# endif

//typedef signed long long int64_t;
typedef unsigned char uint8_t; 
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;


#if __WORDSIZE == 64
typedef unsigned long int       uint64_t;
#else
__extension__
typedef unsigned long long int  uint64_t;
#endif


#endif
