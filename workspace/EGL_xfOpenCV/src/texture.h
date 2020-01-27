#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#include <png.h>

struct PngInfo
{
    png_uint_32 width;
    png_uint_32 height;
    unsigned char *data;
    bool has_alpha;
};

PngInfo loadPng(std::string filename);
void deletePng(PngInfo &png);

#endif