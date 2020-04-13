#include "ubx.h"

/* define type safe accessors for basic C types */
def_type_accessors(char, char);
def_type_accessors(int, int);
def_type_accessors(uint, unsigned int);
def_type_accessors(long, long);
def_type_accessors(ulong, unsigned long);

def_type_accessors(uint8, uint8_t);
def_type_accessors(uint16, uint16_t);
def_type_accessors(uint32, uint32_t);
def_type_accessors(uint64, uint64_t);

def_type_accessors(int8, int8_t);
def_type_accessors(int16, int16_t);
def_type_accessors(int32, int32_t);
def_type_accessors(int64, int64_t);

def_type_accessors(float, float);
def_type_accessors(double, double);
