#pragma once

#include <stddef.h>
#include<stdint.h>

struct HNode{
    HNode *next = NULL;
    uint64_t hcode = 0;  //hash value
};


struct HTab{
    HNode** tab = NULL; //array of slots
    size_t mask = 0;    // power of two array size
    size_t size = 0;   // keys
};


