#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#define TTFTK_IMPLEMENTATION
#include "ttftk.h"

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

void RenderGlyph(ttftk::TrueTypeFile const& _ttfFile, ttftk::Glyph const& _glyph);

int main(int argc, char const ** argv)
{
    if (argc == 1)
    {
        std::cout << "Missing path to TrueType font file as first argument." << std::endl;
        return 1;
    }

    std::vector<uint8_t> memory = LoadFile(argv[1]);
    uint32_t iCharCode = ~0u;
    if (argc > 2)
        iCharCode = strtol(argv[2], nullptr, 16);

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
    else
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

            int32_t windingNumber = 0;
            for (ttftk::GlyphContour const& contour : _glyph.contours)
            {
                for (size_t point = 0u; point < contour.x.size()-2; point+=2)
                {
                    int16_t pointX[3] {
                        (int16_t)(contour.x[point] - sampleX),
                        (int16_t)(contour.x[point + 1] - sampleX),
                        (int16_t)(contour.x[point + 2] - sampleX)
                    };
                    int16_t pointY[3] {
                        (int16_t)(contour.y[point] - sampleY),
                        (int16_t)(contour.y[point + 1] - sampleY),
                        (int16_t)(contour.y[point + 2] - sampleY)
                    };

                    uint8_t key = ((pointY[0] > 0) ? 2 : 0)
                        | ((pointY[1] > 0) ? 4 : 0)
                        | ((pointY[2] > 0) ? 8 : 0);

                    static constexpr uint16_t kLUT = 0x2E74u;

                    uint16_t intType = kLUT >> key;
                    if (intType & 3)
                    {
                        float a = (float)pointY[0] - 2.f * (float)pointY[1] + (float)pointY[2];
                        float b = (float)pointY[0] - (float)pointY[1];
                        float c = (float)pointY[0];

                        float cx0 = -1.f;
                        float cx1 = -1.f;
                        if (std::abs(a) < 0.001f)
                        {
                            float t = c / (2.f * b);
                            float cx = (pointX[0] - 2.f*pointX[1] + pointX[2])*t*t
                                - 2.f*(pointX[0] - pointX[1])*t
                                + pointX[0];
                            if (intType & 1)
                                cx0 = cx;
                            if (intType & 2)
                                cx1 = cx;
                        }
                        else
                        {
                            if (intType & 1)
                            {
                                float t0 = (b - std::sqrt(b*b - a*c)) / a;
                                cx0 = (pointX[0] - 2.f*pointX[1] + pointX[2])*t0*t0
                                    - 2.f*(pointX[0] - pointX[1])*t0
                                    + pointX[0];
                            }
                            if (intType & 2)
                            {
                                float t1 = (b + std::sqrt(b*b - a*c)) / a;
                                cx1 = (pointX[0] - 2.f*pointX[1] + pointX[2])*t1*t1
                                    - 2.f*(pointX[0] - pointX[1])*t1
                                    + pointX[0];
                            }
                        }

                        if (cx0 >= 0.f) ++windingNumber;
                        if (cx1 >= 0.f) --windingNumber;
                    }
                }
            }

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
