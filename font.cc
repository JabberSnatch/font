#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#define TTFTK_IMPLEMENTATION
#include "ttftk.h"

#define BMPTK_IMPLEMENTATION
#include "bmptk/bmptk.h"

std::vector<uint8_t> LoadFile(char const* _path)
{
    std::ifstream source_file(_path, std::ios_base::binary);

    source_file.seekg(0, std::ios_base::end);
    size_t size = source_file.tellg();
    source_file.seekg(0, std::ios_base::beg);

    std::vector<uint8_t> memory{};
    memory.resize(size);

    source_file.read((char*)memory.data(), size);

    return memory;
}

void WriteFile(char const* _path, uint8_t const* _base, uint32_t _size)
{
    std::ofstream dest_file(_path, std::ios_base::binary);
    dest_file.write((char const*)_base, (std::streamsize)_size);
}

void RenderGlyph(ttftk::TrueTypeFile const& _ttfFile, ttftk::Glyph const& _glyph);
void RenderGlyph(ttftk::TrueTypeFile const& _ttfFile, ttftk::Glyph const& _glyph,
                 bmptk::BitmapV1Header const& _header, bmptk::PixelValue *_pixels,
                 uint32_t xres, uint32_t yres, uint32_t xOffset, uint32_t yOffset);

int main(int argc, char const ** argv)
{
    if (argc == 1)
    {
        std::cout << "Missing path to TrueType font file as first argument." << std::endl;
        return 1;
    }

    std::vector<uint8_t> memory = LoadFile(argv[1]);
    uint32_t iCharCode = ~0u;
    uint32_t glyphCountX = 10;
    uint32_t glyphCountY = 10;

    if (argc == 3)
        iCharCode = strtol(argv[2], nullptr, 16);
    else if (argc > 3)
    {
        glyphCountX = strtol(argv[2], nullptr, 10);
        glyphCountY = strtol(argv[3], nullptr, 10);
    }

    ttftk::TrueTypeFile ttfFile{};
    if (ttftk::LoadTTF(memory.data(), &ttfFile) != ttftk::Result::Success)
    {
        std::cout << "error parsing ttf" << std::endl;
        return 1;
    }

    ttftk::Glyph glyph{};
    if (iCharCode != ~0u)
    {
        if (ttftk::ReadGlyphData(ttfFile, iCharCode, &glyph) != ttftk::Result::Success)
        {
            std::cout << "error reading glyph data" << std::endl;
            return 1;
        }
        RenderGlyph(ttfFile, glyph);
    }
    else if (argc == 2)
    {
        std::vector<uint32_t> charList = ttftk::ListCharCodes(ttfFile);
        for (uint32_t charCode : charList)
        {
            std::cout << "================================================================================" << std::endl;
            std::cout << std::hex << charCode << std::endl;
            ttftk::ReadGlyphData(ttfFile, charCode, &glyph);
            RenderGlyph(ttfFile, glyph);
        }
    }
    else
    {
        uint32_t charListOffset = 0u;
        if (argc > 5)
            charListOffset = std::strtol(argv[5], nullptr, 10);

        uint32_t boxsize = 75;

        bmptk::BitmapV1Header header{};
        header.width = boxsize * glyphCountX;
        header.height = -boxsize * glyphCountY;

        std::vector<bmptk::PixelValue> pixels(boxsize*boxsize * glyphCountX * glyphCountY);
        std::memset(pixels.data(), 0, sizeof(bmptk::PixelValue)*pixels.size());
        std::vector<uint32_t> charList = ttftk::ListCharCodes(ttfFile);

        uint32_t glyphX = 0;
        uint32_t glyphY = 0;
        for (std::size_t index = 0u; index < charList.size()-charListOffset; ++index)
        {
            uint32_t charCode = charList[index + charListOffset];
            ttftk::ReadGlyphData(ttfFile, charCode, &glyph);
            RenderGlyph(ttfFile, glyph, header, pixels.data(),
                        boxsize, boxsize, glyphX * boxsize, glyphY * boxsize);
            ++glyphX;
            if (glyphX >= glyphCountX)
            {
                glyphX = 0;
                ++glyphY;
                if (glyphY >= glyphCountY)
                    break;
            }
        }

        std::vector<uint8_t> memory(bmptk::AllocSize(&header));
        bmptk::WriteBMP(&header, pixels.data(), memory.data());
        char const* outpath = "testfile.bmp";
        if (argc > 4)
            outpath = argv[4];
        WriteFile(outpath, memory.data(), memory.size());
    }

    return 0;
}

void RenderGlyph(ttftk::TrueTypeFile const& _ttfFile, ttftk::Glyph const& _glyph)
{
    int maxX = 80;
    int maxY = 40;

#if 0
    float sourceMaxX = (float)_glyph.xmax;
    float sourceMinX = (float)_glyph.xmin;
    float sourceMaxY = (float)_glyph.ymax;
    float sourceMinY = (float)_glyph.ymin;
#elif 0
    float sourceMaxX = (float)_ttfFile.emsize;
    float sourceMinX = (float)0;
    float sourceMaxY = (float)_ttfFile.emsize;
    float sourceMinY = (float)0;
#else
    float sourceMaxX = (float)_ttfFile.xmax;
    float sourceMinX = (float)_ttfFile.xmin;
    float sourceMaxY = (float)_ttfFile.ymax;
    float sourceMinY = (float)_ttfFile.ymin;
#endif

    float yaspect = 1.f;
    float xaspect = (sourceMaxX - sourceMinX) / (sourceMaxY - sourceMinY);
    if (xaspect > 1.f)
    {
        yaspect = 1.f / xaspect;
        xaspect = 1.f;
    }

    for (int y = 0; y < maxY; ++y)
    {
        for (int x = 0; x < maxX; ++x)
        {
            float u = ((float)x / (float)maxX) / xaspect;
            float v = ((float)(maxY-y) / (float)maxY) / yaspect;

            int16_t sampleX = (int16_t)std::round(u * (sourceMaxX - sourceMinX) + sourceMinX);
            int16_t sampleY = (int16_t)std::round(v * (sourceMaxY - sourceMinY) + sourceMinY);

            int32_t windingNumber = ttftk::EvalWindingNumber(&_glyph, sampleX, sampleY);

#if 1
            if (windingNumber > 0)
                std::cout << "X";
#if 0
            else if (windingNumber < 0)
                std::cout << -windingNumber;
#endif
            else
                std::cout << " ";
#else
            std::cout << windingNumber;
#endif
        }
        std::cout << std::endl;
    }
}

void RenderGlyph(ttftk::TrueTypeFile const& _ttfFile, ttftk::Glyph const& _glyph,
                 bmptk::BitmapV1Header const& _header, bmptk::PixelValue *_pixels,
                 uint32_t xres, uint32_t yres, uint32_t xOffset, uint32_t yOffset)
{
    int maxX = (int)xres;
    int maxY = (int)yres;

    float sourceMaxX = (float)_ttfFile.xmax;
    float sourceMinX = (float)_ttfFile.xmin;
    float sourceMaxY = (float)_ttfFile.ymax;
    float sourceMinY = (float)_ttfFile.ymin;

    float yaspect = 1.f;
    float xaspect = (sourceMaxX - sourceMinX) / (sourceMaxY - sourceMinY);
    if (xaspect > 1.f)
    {
        yaspect = 1.f / xaspect;
        xaspect = 1.f;
    }

    for (int y = 0; y < maxY; ++y)
    {
        for (int x = 0; x < maxX; ++x)
        {
            float u = ((float)x / (float)maxX) / xaspect;
            float v = ((float)(maxY-y) / (float)maxY) / yaspect;

            int16_t sampleX = (int16_t)std::round(u * (sourceMaxX - sourceMinX) + sourceMinX);
            int16_t sampleY = (int16_t)std::round(v * (sourceMaxY - sourceMinY) + sourceMinY);

            bmptk::PixelValue* pixel = _pixels + ((xOffset + x) + (yOffset + y)*_header.width);

            int32_t windingNumber = ttftk::EvalWindingNumber(&_glyph, sampleX, sampleY);
            if (windingNumber > 0)
                pixel->d[0] = pixel->d[1] = pixel->d[2] = 150;
            else
            {
                float distance = ttftk::EvalDistance(&_glyph, sampleX, sampleY);
                if (distance < 20.f)
                    pixel->d[0] = pixel->d[1] = pixel->d[2] = 0;
                else
                    pixel->d[0] = pixel->d[1] = pixel->d[2] = 255;
            }
        }
    }
}
