/*
    Compiles the stb_image implementation (https://github.com/nothings/stb).

    stb_image is a single-header library: every file may include it for the
    declarations, but exactly one .cpp must define STB_IMAGE_IMPLEMENTATION
    before including it. Keeping it in its own file means editing viewer.cpp
    doesn't recompile ~8000 lines of image decoder every time.
*/

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG       // the table only ships PNGs; drop these
#define STBI_ONLY_JPEG      // two lines to enable every format
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include <common/stb_image.h>
