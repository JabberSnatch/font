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
                 uint32_t xres, uint32_t yres, uint32_t xOffset, uint32_t yOffset,
                 uint32_t samplingRate, float pixelSize, bool subPixelEval);

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
        uint32_t const ppem = (argc > 5)
            ? std::strtol(argv[5], nullptr, 10)
            : 12;

        uint32_t const samplingRate = (argc > 6)
            ? std::strtol(argv[6], nullptr, 10)
            : 0u;

        uint32_t const subPixelEval = (argc > 7)
            ? std::strtol(argv[7], nullptr, 10)
            : 0u;

        uint32_t const charListOffset = (argc > 8)
            ? std::strtol(argv[8], nullptr, 10)
            : 0u;


        float const xtoemRatio = (float)(ttfFile.xmax - ttfFile.xmin) / (float)ttfFile.emsize;
        float const ytoemRatio = (float)(ttfFile.ymax - ttfFile.ymin) / (float)ttfFile.emsize;
        uint32_t const gridSizeX = (uint32_t)std::round(xtoemRatio * (float)ppem);
        uint32_t const gridSizeY = (uint32_t)std::round(ytoemRatio * (float)ppem);
        float const pixelSize = (float)ttfFile.emsize / (float)ppem;

        bmptk::BitmapV1Header header{};
        header.width = gridSizeX * glyphCountX;
        header.height = -gridSizeY * glyphCountY;

        std::vector<bmptk::PixelValue> pixels(std::abs(header.width * header.height));
        std::memset(pixels.data(), 0, sizeof(bmptk::PixelValue)*pixels.size());
        std::vector<uint32_t> charList = ttftk::ListCharCodes(ttfFile);
        bmptk::PixelValue* const pixelBuffer = pixels.data();

        uint32_t glyphX = 0;
        uint32_t glyphY = 0;
        for (std::size_t index = 0u; index < charList.size()-charListOffset; ++index)
        {
            uint32_t charCode = charList[index + charListOffset];
            ttftk::ReadGlyphData(ttfFile, charCode, &glyph);
            RenderGlyph(ttfFile, glyph, header, pixelBuffer,
                        gridSizeX, gridSizeY, glyphX * gridSizeX, glyphY * gridSizeY,
                        samplingRate, pixelSize, !!subPixelEval);

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

            int32_t windingNumber = ttftk::EvalWindingNumber(&_glyph, sampleX, sampleY, nullptr);

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
                 uint32_t xres, uint32_t yres, uint32_t xOffset, uint32_t yOffset,
                 uint32_t samplingRate, float pixelSize, bool subPixelEval)
{
    int const maxX = (int)xres;
    int const maxY = (int)yres;

    float const sourceMaxX = (float)_ttfFile.xmax;
    float const sourceMinX = (float)_ttfFile.xmin;
    float const sourceMaxY = (float)_ttfFile.ymax;
    float const sourceMinY = (float)_ttfFile.ymin;

    float yaspect = 1.f;
    float xaspect = (sourceMaxX - sourceMinX) / (sourceMaxY - sourceMinY);
    if (xaspect > 1.f)
    {
        yaspect = 1.f / xaspect;
        xaspect = 1.f;
    }

    float const ssMaxX = (float)(maxX << samplingRate);
    float const ssMaxY = (float)(maxY << samplingRate);
    pixelSize /= (float)(1 << samplingRate);

    for (int y = 0; y < maxY; ++y)
    {
        for (int x = 0; x < maxX; ++x)
        {
            float accum = 0.f;
            uint32_t sampleCount = (1 << (samplingRate*2));
            for (uint32_t s = 0; s < sampleCount; ++s)
            {
                int sx = s & ((1 << samplingRate) - 1);
                int sy = (s & (((1 << samplingRate) - 1) << samplingRate)) >> samplingRate;

                float u = (((float)((x << samplingRate) + sx) + 0.5f) / ssMaxX) / xaspect;
                float v = (1.f - ((float)((y << samplingRate) + sy) + 0.5f) / ssMaxY) / yaspect;

                int16_t sampleX = (int16_t)std::round(u * (sourceMaxX - sourceMinX) + sourceMinX);
                int16_t sampleY = (int16_t)std::round(v * (sourceMaxY - sourceMinY) + sourceMinY);

                float coverage = 0.f;
                float distance = pixelSize*0.5f;
                int32_t windingNumber =
                    ttftk::EvalWindingNumber(&_glyph, sampleX, sampleY,
                                             (subPixelEval ? &distance : nullptr));

                if (windingNumber > 0)
                    coverage = 1.f-std::max(0.5f-(std::abs(distance)/pixelSize), 0.f);
                else if (std::abs(distance) < pixelSize*0.5f)
                    coverage = std::max(0.5f-(std::abs(distance) / pixelSize), 0.f);

                accum += (255.f * coverage) / sampleCount;

#if 0
                if (windingNumber > 0)
                    accum += 255.f / sampleCount;
#if 0
                else
                {
                    float distance = ttftk::EvalDistance(&_glyph, sampleX, sampleY);
                    if (distance < 300.f)
                        accum += 0.f / sampleCount;
                    else
                        accum += 0.f / sampleCount;
                }
#else
                else accum += 0.f / sampleCount;
#endif
#endif
            }

            bmptk::PixelValue* pixel = _pixels + ((xOffset + x) + (yOffset + y)*_header.width);
            pixel->d[0] = pixel->d[1] = pixel->d[2] = (float)std::round(accum);
        }
    }
}
