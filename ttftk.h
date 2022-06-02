#pragma once

#include <cstdint>

namespace ttftk
{

enum class Result
{
    Success,
    UnknownScalerType,
    UnknownCMAPTable,
    UnknownCMAPFormat,
    GlyphMissing,
};

struct OffsetSubtable
{
    uint32_t scalerType;
    uint16_t numTables;
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
};

struct TableDirectoryEntry
{
    uint32_t tag;
    uint32_t checkSum;
    uint32_t offset;
    uint32_t length;
};

struct RequiredTables
{
    TableDirectoryEntry const *cmap, *glyf, *head, *hhea, *hmtx, *loca, *maxp, *name, *post;
};

struct OptionalTables
{
    TableDirectoryEntry const *cvt, *fpgm, *hdmx, *kern, *os2, *prep;
};

struct TrueTypeFile
{
    uint8_t const* memory;
    OffsetSubtable offsets;
    RequiredTables required;
    OptionalTables optional;
    std::vector<TableDirectoryEntry> tableDirectory;
};

struct GlyphPoints
{
    int16_t xmin, ymin, xmax, ymax;
    uint16_t pointCount;
    std::vector<uint16_t> endPoints{};
    std::vector<uint8_t> contourFlags{};
    std::vector<int16_t> contourX{};
    std::vector<int16_t> contourY{};
};

struct GlyphContour
{
    std::vector<int16_t> x;
    std::vector<int16_t> y;
};

struct Glyph
{
    int16_t xmin, ymin, xmax, ymax;
    std::vector<GlyphContour> contours;
};

Result LoadTTF(uint8_t const* _memory, TrueTypeFile* _ttfFile);
Result ReadGlyphData(TrueTypeFile const& _ttfFile, uint16_t _characterCode, Glyph* _glyph);

void const* ExtractOffsetSubtable(void const* _ptr, OffsetSubtable& _output);
void const* ExtractTableDirectory(void const* _ptr, uint16_t _count, TableDirectoryEntry* _output);
uint32_t ExtractGlyphIndex(void const* _ptr, uint16_t _format, uint16_t _charCode);
GlyphPoints ExtractGlyphPoints(uint8_t const* _loca,
                               uint8_t const* _glyf,
                               uint16_t _indexToLocFormat,
                               uint32_t _glyphIndex);

template <typename T>
static inline void const* AdvancePointer(void const* _source, size_t _count = 1)
{
    return ((T const*)_source) + _count;
}

static inline void ReadTag(void const*& _ptr, uint8_t* _tag)
{
    uint8_t const* base = (uint8_t const*)_ptr;
    _tag[0] = base[0];
    _tag[1] = base[1];
    _tag[2] = base[2];
    _tag[3] = base[3];
    _ptr = AdvancePointer<uint8_t>(_ptr, 4);
}

static inline uint8_t ReadU8(void const*& _ptr) {
    uint8_t v = *(uint8_t const*)_ptr;
    _ptr = AdvancePointer<uint8_t>(_ptr);
    return v;
}

static inline int8_t ReadS8(void const*& _ptr) {
    int8_t v = *(int8_t const*)_ptr;
    _ptr = AdvancePointer<int8_t>(_ptr);
    return v;
}

static inline uint16_t ReadU16(void const*& _ptr) {
    uint8_t const* base = (uint8_t const*)_ptr;
    _ptr = AdvancePointer<uint16_t>(_ptr);
    return (base[0]<<8) | base[1];
}

static inline void const* ReadU16(void const* _ptr, uint16_t* _dst, size_t _count) {
    for (size_t index = 0u; index < _count; ++index) {
        _dst[index] = ReadU16(_ptr);
    }
    return _ptr;
}

static inline int16_t ReadS16(void const*& _ptr) {
    return (int16_t)ReadU16(_ptr);
}

static inline uint32_t ReadU32(void const*& _ptr) {
    uint8_t const* base = (uint8_t const*)_ptr;
    _ptr = AdvancePointer<uint32_t>(_ptr);
    return (base[0]<<24) | (base[1]<<16) | (base[2]<<8) | base[3];
}

static inline int32_t ReadS32(void const*& _ptr) {
    return (int32_t)ReadU32(_ptr);
}

static inline uint64_t ReadU64(void const*& _ptr) {
    uint64_t lo = ReadU32(_ptr);
    uint64_t hi = ReadU32(_ptr);
    return (hi << 32) | lo;
}

static inline float F2Dot14(int16_t _v) {
    return (float)_v / 16384.f;
}

#define CompareTag(t, s) (t[3]==s[0]) && (t[2]==s[1]) && (t[1]==s[2]) && (t[0]==s[3])
#define CompareTagU32(t, s) CompareTag(((char const*)&t), s)

#ifdef TTFTK_IMPLEMENTATION

Result LoadTTF(uint8_t const* _memory, TrueTypeFile* _ttfFile)
{
    TrueTypeFile ttfFile{};
    ttfFile.memory = _memory;

    void const* ptr = (void const*)ttfFile.memory;
    ptr = ExtractOffsetSubtable(ptr, ttfFile.offsets);

    if (!(ttfFile.offsets.scalerType == 0x00010000 // Windows/Adobe ttf
          || ttfFile.offsets.scalerType == 0x74727565)) // "true", OSX/iOS ttf
        return Result::UnknownScalerType;

    ttfFile.tableDirectory.resize(ttfFile.offsets.numTables);
    ptr = ExtractTableDirectory(ptr, ttfFile.offsets.numTables, ttfFile.tableDirectory.data());

    for (uint32_t index = 0u; index < ttfFile.offsets.numTables; ++index)
    {
        TableDirectoryEntry const& entry = ttfFile.tableDirectory[index];

        if (CompareTagU32(entry.tag, "cmap"))
            ttfFile.required.cmap = &entry;
        else if (CompareTagU32(entry.tag, "glyf"))
            ttfFile.required.glyf = &entry;
        else if (CompareTagU32(entry.tag, "head"))
            ttfFile.required.head = &entry;
        else if (CompareTagU32(entry.tag, "hhea"))
            ttfFile.required.hhea = &entry;
        else if (CompareTagU32(entry.tag, "hmtx"))
            ttfFile.required.hmtx = &entry;
        else if (CompareTagU32(entry.tag, "loca"))
            ttfFile.required.loca = &entry;
        else if (CompareTagU32(entry.tag, "maxp"))
            ttfFile.required.maxp = &entry;
        else if (CompareTagU32(entry.tag, "name"))
            ttfFile.required.name = &entry;
        else if (CompareTagU32(entry.tag, "post"))
            ttfFile.required.post = &entry;

        else if (CompareTagU32(entry.tag, "cvt "))
            ttfFile.optional.cvt = &entry;
        else if (CompareTagU32(entry.tag, "fpgm"))
            ttfFile.optional.fpgm = &entry;
        else if (CompareTagU32(entry.tag, "hdmx"))
            ttfFile.optional.hdmx = &entry;
        else if (CompareTagU32(entry.tag, "kern"))
            ttfFile.optional.kern = &entry;
        else if (CompareTagU32(entry.tag, "OS/2"))
            ttfFile.optional.os2 = &entry;
        else if (CompareTagU32(entry.tag, "prep"))
            ttfFile.optional.prep = &entry;
    }

    {
        uint8_t const* cmapBase = ttfFile.memory + ttfFile.required.cmap->offset;
        ptr = (void const*)cmapBase;

        uint16_t cmapversion = ReadU16(ptr);
        uint16_t tableCount = ReadU16(ptr);

        uint32_t unicodeTableOffset = 0u;
        uint16_t unicodeVariant = 0u;
        for (uint16_t index = 0u; index < tableCount; ++index)
        {
            uint16_t platformID = ReadU16(ptr);
            uint16_t platformSpecificID = ReadU16(ptr);
            uint32_t offset = ReadU32(ptr);

            if ((platformID == 0u
                 && platformSpecificID != 14)
                || (platformID == 3u
                    && (platformSpecificID == 10 || platformSpecificID == 1)))
            {
                unicodeTableOffset = offset;
                unicodeVariant = platformSpecificID;
                break;
            }
        }

        if (unicodeTableOffset == 0u)
            return Result::UnknownCMAPTable;

        uint8_t const* cmapSubtableBase = cmapBase + unicodeTableOffset;
        ptr = (void const*)cmapSubtableBase;
        uint16_t format = ReadU16(ptr);

        if (format != 4 && format != 12)
            return Result::UnknownCMAPFormat;
    }

    std::swap(ttfFile, *_ttfFile);
    return Result::Success;
}

Result ReadGlyphData(TrueTypeFile const& _ttfFile, uint16_t _characterCode, Glyph* _glyph)
{
    void const* ptr = nullptr;

    uint32_t glyphIndex = 0u;
    {
        uint8_t const* cmapBase = _ttfFile.memory + _ttfFile.required.cmap->offset;
        ptr = (void const*)cmapBase;

        uint16_t cmapversion = ReadU16(ptr);
        uint16_t tableCount = ReadU16(ptr);

        for (uint16_t index = 0u; index < tableCount; ++index)
        {
            uint16_t platformID = ReadU16(ptr);
            uint16_t platformSpecificID = ReadU16(ptr);
            uint32_t offset = ReadU32(ptr);

            if ((platformID == 0u
                 && platformSpecificID != 14)
                || (platformID == 3u
                    && (platformSpecificID == 10
                        || platformSpecificID == 1)))
            {
                uint32_t unicodeTableOffset = offset;
                uint16_t unicodeVariant = platformSpecificID;

                uint8_t const* cmapSubtableBase = cmapBase + unicodeTableOffset;
                ptr = (void const*)cmapSubtableBase;
                uint16_t format = ReadU16(ptr);

                if (format != 4 && format != 12)
                    continue;

                glyphIndex = ExtractGlyphIndex(ptr, format, _characterCode);
                if (glyphIndex != 0u)
                    break;
            }
        }
    }

    if (glyphIndex == 0u)
        return Result::GlyphMissing;

    {
        uint8_t const* maxpBase = _ttfFile.memory + _ttfFile.required.maxp->offset;
        ptr = AdvancePointer<uint16_t>(maxpBase);
        uint16_t glyphCount = ReadU16(ptr);
        uint16_t maxPoints = ReadU16(ptr);
        uint16_t maxContours = ReadU16(ptr);
        uint16_t maxComponentPoints = ReadU16(ptr);
        uint16_t maxComponentContours = ReadU16(ptr);
        ptr = AdvancePointer<uint16_t>(ptr, 7);
        uint16_t maxComponentElements = ReadU16(ptr);
        uint16_t maxComponentDepth = ReadU16(ptr);

        uint8_t const* headBase = _ttfFile.memory + _ttfFile.required.head->offset;
        ptr = headBase;
        uint32_t headversion = ReadU32(ptr);
        uint32_t fontRevision = ReadU32(ptr);
        uint32_t checkSumAdjustment = ReadU32(ptr);
        uint32_t magicNumber = ReadU32(ptr);
        uint16_t headFlags = ReadU16(ptr);
        uint16_t unitsPerEm = ReadU16(ptr);
        uint64_t createdTime = ReadU64(ptr);
        uint64_t modifiedTime = ReadU64(ptr);
        int16_t xMin = ReadS16(ptr);
        int16_t yMin = ReadS16(ptr);
        int16_t xMax = ReadS16(ptr);
        int16_t yMax = ReadS16(ptr);
        uint16_t macStyle = ReadU16(ptr);
        uint16_t lowestRecPPEM = ReadU16(ptr);
        int16_t fontDirectionHint = ReadS16(ptr);
        int16_t indexToLocFormat = ReadS16(ptr);
        int16_t glyphDataFormat = ReadS16(ptr);

        uint8_t const* locaBase = _ttfFile.memory + _ttfFile.required.loca->offset;
        uint8_t const* glyfBase = _ttfFile.memory + _ttfFile.required.glyf->offset;

        GlyphPoints points = ExtractGlyphPoints(locaBase, glyfBase, indexToLocFormat, glyphIndex);

        _glyph->xmin = points.xmin;
        _glyph->xmax = points.xmax;
        _glyph->ymin = points.ymin;
        _glyph->ymax = points.ymax;
        std::vector<GlyphContour>& contours = _glyph->contours;

        // Converting to a full quadratic data layout.
        // Should be a sequence of on/off/on/off.../on points, repeating the first point at the end.
        std::vector<int16_t> contourQuadX{};
        contourQuadX.reserve(points.pointCount + points.pointCount / 2 + 1);
        std::vector<int16_t> contourQuadY{};
        contourQuadY.reserve(points.pointCount + points.pointCount / 2 + 1);

        contours.resize(points.endPoints.size());

        for (uint16_t contourIndex = 0u, beginPoint = 0u;
             contourIndex < points.endPoints.size(); ++contourIndex)
        {
            GlyphContour& contour = contours[contourIndex];

            uint16_t endPoint = points.endPoints[contourIndex] + 1;

            uint8_t lastFlags = 0u;
            for (uint16_t pointIndex = beginPoint; pointIndex < endPoint; ++pointIndex)
            {
                uint8_t flags = points.contourFlags[pointIndex];
                if ((lastFlags ^ flags) & 1)
                {
                    contour.x.push_back(points.contourX[pointIndex]);
                    contour.y.push_back(points.contourY[pointIndex]);
                }
                else
                {
                    int16_t avgX = (points.contourX[pointIndex-1] + points.contourX[pointIndex]) / 2;
                    int16_t avgY = (points.contourY[pointIndex-1] + points.contourY[pointIndex]) / 2;
                    contour.x.push_back(avgX);
                    contour.y.push_back(avgY);
                    contour.x.push_back(points.contourX[pointIndex]);
                    contour.y.push_back(points.contourY[pointIndex]);
                }

                lastFlags = flags;
            }

            if (!((points.contourFlags[endPoint-1] ^ points.contourFlags[beginPoint]) & 1))
            {
                int16_t avgX = (points.contourX[endPoint-1] + points.contourX[beginPoint]) / 2;
                int16_t avgY = (points.contourY[endPoint-1] + points.contourY[beginPoint]) / 2;
                contour.x.push_back(avgX);
                contour.y.push_back(avgY);
            }

            contour.x.push_back(points.contourX[beginPoint]);
            contour.y.push_back(points.contourY[beginPoint]);
            beginPoint = endPoint;
        }
    }

    return Result::Success;
}

void const* ExtractOffsetSubtable(void const* _ptr, OffsetSubtable& _output)
{
    void const* nextPtr = AdvancePointer<OffsetSubtable>(_ptr);
    _output.scalerType = ReadU32(_ptr);
    _output.numTables = ReadU16(_ptr);
    _output.searchRange = ReadU16(_ptr);
    _output.entrySelector = ReadU16(_ptr);
    _output.rangeShift = ReadU16(_ptr);
    return nextPtr;
}

void const* ExtractTableDirectory(void const* _ptr, uint16_t _count, TableDirectoryEntry* _output)
{
    void const* nextPtr = AdvancePointer<TableDirectoryEntry>(_ptr, (size_t)_count);
    for (uint16_t index = 0u; index < _count; ++index)
    {
        void const* ptr = AdvancePointer<TableDirectoryEntry>(_ptr);
        _output[index].tag = ReadU32(_ptr);
        _output[index].checkSum = ReadU32(_ptr);
        _output[index].offset = ReadU32(_ptr);
        _output[index].length = ReadU32(_ptr);
        _ptr = ptr;
    }
    return nextPtr;
}

uint32_t ExtractGlyphIndex(void const* _ptr, uint16_t _format, uint16_t _charCode)
{
    uint32_t glyphIndex = 0u;

    if (_format == 4)
    {
        uint16_t length = ReadU16(_ptr);
        uint16_t language = ReadU16(_ptr);
        uint16_t segCount = ReadU16(_ptr) / 2;
        uint16_t searchRange = ReadU16(_ptr);
        uint16_t entrySelector = ReadU16(_ptr);
        uint16_t rangeShift = ReadU16(_ptr);
        std::vector<uint16_t> endCode(segCount);
        _ptr = ReadU16(_ptr, endCode.data(), endCode.size());
        uint16_t reservedPad = ReadU16(_ptr);
        std::vector<uint16_t> startCode(segCount);
        _ptr = ReadU16(_ptr, startCode.data(), startCode.size());
        std::vector<uint16_t> idDelta(segCount);
        _ptr = ReadU16(_ptr, idDelta.data(), idDelta.size());
        std::vector<uint16_t> idRangeOffset(segCount);
        _ptr = ReadU16(_ptr, idRangeOffset.data(), idRangeOffset.size());
        uint16_t const* glyphIndexArrayBase = (uint16_t const*)_ptr;

        //std::cout << std::dec << "seg count " << segCount << std::endl;
        for (uint16_t index = 0u; index < segCount; ++index)
        {
            if (endCode[index] >= _charCode)
            {
                if (startCode[index] > _charCode)
                {
                    //std::cout << "missing glyph" << std::endl;
                    break;
                }

                /*
                std::cout << "glyph page location " << index << std::endl;
                std::cout << std::hex
                          << "page begin " << startCode[index] << std::endl
                          << "page end " << endCode[index] << std::endl
                          << "idRangeOffset " << idRangeOffset[index] << std::endl
                          << "idDelta " << idDelta[index] << std::endl;
                */

                if (!idRangeOffset[index])
                    glyphIndex = (uint32_t)(idDelta[index] + _charCode) & 0xffff;
                else
                {
                    uint16_t offset = idRangeOffset[index]/2 + (_charCode - startCode[index]);
                    void const* pIndex = glyphIndexArrayBase + offset + index - segCount;
                    glyphIndex = (uint32_t)ReadU16(pIndex);
                }

                break;
            }
        }
        //std::cout << std::dec << "glyph index " << glyphIndex << std::endl;
    }

    if (_format == 12)
    {
        ReadU16(_ptr); // reserved u16
        uint32_t length = ReadU32(_ptr);
        uint32_t language = ReadU32(_ptr);
        uint32_t nGroups = ReadU32(_ptr);

        //std::cout << std::hex << "looking for " << _charCode << std::endl;
        for (uint32_t index = 0u; index < nGroups; ++index)
        {
            uint32_t startCharCode = ReadU32(_ptr);
            uint32_t endCharCode = ReadU32(_ptr);
            uint32_t startGlyphCode = ReadU32(_ptr);

            /*
            std::cout << std::hex
                      << "start code " << startCharCode << std::endl
                      << "end code " << endCharCode << std::endl
                      << "start glyph " << startGlyphCode << std::endl;
            */

            if (_charCode >= startCharCode && _charCode <= endCharCode)
            {
                glyphIndex = startGlyphCode + (_charCode - startCharCode);
                break;
            }
        }
        //std::cout << std::dec << "glyph index " << glyphIndex << std::endl;
    }

    return glyphIndex;
}

GlyphPoints ExtractGlyphPoints(uint8_t const* _loca,
                               uint8_t const* _glyf,
                               uint16_t _indexToLocFormat,
                               uint32_t _glyphIndex)
{
    GlyphPoints output{};

    uint32_t glyphOffset = 0u;
    if (_indexToLocFormat == 0)
    {
        void const* ptr = _loca + _glyphIndex * 2;
        glyphOffset = ReadU16(ptr) * 2;
    }
    else
    {
        void const* ptr = _loca + _glyphIndex * 4;
        glyphOffset = ReadU32(ptr);
    }

    void const* ptr = _glyf + glyphOffset;
    int16_t numberOfContours = ReadS16(ptr);
    output.xmin = ReadS16(ptr);
    output.ymin = ReadS16(ptr);
    output.xmax = ReadS16(ptr);
    output.ymax = ReadS16(ptr);

    if (numberOfContours > 0)
    {
        output.endPoints.resize(numberOfContours);
        ptr = ReadU16(ptr, output.endPoints.data(), output.endPoints.size());
        uint16_t instructionLength = ReadU16(ptr);
        uint8_t const* instructions = (uint8_t const*)ptr;
        ptr = AdvancePointer<uint8_t>(ptr, instructionLength);
        uint8_t const* flagsArray = (uint8_t const*)ptr;

        output.pointCount = output.endPoints[output.endPoints.size()-1] + 1;
        output.contourFlags.resize(output.pointCount);
        output.contourX.resize(output.pointCount);
        output.contourY.resize(output.pointCount);

        uint16_t pointIndex = 0u;
        while (pointIndex < output.pointCount)
        {
            uint8_t flags = *flagsArray++;
            if (flags & 8)
            {
                uint8_t repeatCount = 1 + *flagsArray++;
                std::memset(&output.contourFlags[pointIndex], flags, repeatCount);
                pointIndex += repeatCount;
            }
            else
                output.contourFlags[pointIndex++] = flags;
        }

        void const* xArray = (void const*)flagsArray;
        int16_t x = 0;
        for (pointIndex = 0u; pointIndex < output.pointCount; ++pointIndex)
        {
            int16_t dx = 0;
            if (output.contourFlags[pointIndex] & 2)
            {
                dx = ReadU8(xArray);
                if (!(output.contourFlags[pointIndex] & 16))
                    dx = -dx;
            }
            else if (!(output.contourFlags[pointIndex] & 16))
                    dx = ReadS16(xArray);

            x += dx;
            output.contourX[pointIndex] = x;
        }

        void const* yArray = (void const*)xArray;
        int16_t y = 0;
        for (pointIndex = 0u; pointIndex < output.pointCount; ++pointIndex)
        {
            int16_t dy = 0;
            if (output.contourFlags[pointIndex] & 4)
            {
                dy = ReadU8(yArray);
                if (!(output.contourFlags[pointIndex] & 32))
                    dy = -dy;
            }
            else if (!(output.contourFlags[pointIndex] & 32))
                    dy = ReadS16(yArray);

            y += dy;
            output.contourY[pointIndex] = y;
        }
    }

    else if (numberOfContours < 0)
    {
        output.pointCount = 0u;

        //std::cout << "composite glyph" << std::endl;
        uint16_t flags = 32;
        while (flags & 32)
        {
            float a = 1.f, b = 0.f, c = 0.f, d = 1.f, e = 0.f, f = 0.f;

            flags = ReadU16(ptr);
            uint16_t componentIndex = ReadU16(ptr);
            switch (flags & 3)
            {
            case 0:
            case 1:
            {
                //std::cout << "matching points unsupported" << std::endl;
                return{};
            } break;
            case 2:
            {
                e = (float)ReadS8(ptr);
                f = (float)ReadS8(ptr);
            } break;
            case 3:
            {
                e = (float)ReadS16(ptr);
                f = (float)ReadS16(ptr);
            } break;
            }

            if (flags & 8)
            {
                a = d = F2Dot14(ReadS16(ptr));
                b = c = 0.f;
            }
            if (flags & 64)
            {
                a = F2Dot14(ReadS16(ptr));
                d = F2Dot14(ReadS16(ptr));
                b = c = 0.f;
            }
            if (flags & 128)
            {
                a = F2Dot14(ReadS16(ptr));
                b = F2Dot14(ReadS16(ptr));
                c = F2Dot14(ReadS16(ptr));
                d = F2Dot14(ReadS16(ptr));
            }

            // These are not the formulas documented on Apple's website,
            // other sources claim that they are the correct ones.
            // http://pfaedit.sourceforge.net/Composites/index.html
            float m = std::sqrt(a*a + b*b);
            float n = std::sqrt(c*c + d*d);

            bool windingFlip = (a*d - b*c < 0.f);

            GlyphPoints points = ExtractGlyphPoints(_loca, _glyf, _indexToLocFormat, componentIndex);
            uint16_t beginRange = output.pointCount;
            output.pointCount += points.pointCount;
            output.contourFlags.resize(output.pointCount);
            output.contourX.resize(output.pointCount);
            output.contourY.resize(output.pointCount);
            output.endPoints.reserve(output.endPoints.size() + points.endPoints.size());
            for (uint16_t endPoint : points.endPoints)
                output.endPoints.push_back(beginRange + endPoint);

            if (!windingFlip)
            {
                for (uint16_t pointIndex = 0u; pointIndex < points.pointCount; ++pointIndex)
                {
                    output.contourFlags[beginRange + pointIndex] = points.contourFlags[pointIndex];

                    int16_t x = points.contourX[pointIndex];
                    int16_t y = points.contourY[pointIndex];
                    output.contourX[beginRange + pointIndex] = (int16_t)(m * (a*x + c*y + e));
                    output.contourY[beginRange + pointIndex] = (int16_t)(n * (b*x + d*y + f));
                }
            }

            else
            {
                for (uint16_t pointIndex = 0u; pointIndex < points.pointCount; ++pointIndex)
                {
                    output.contourFlags[beginRange + pointIndex] =
                        points.contourFlags[points.pointCount-1 - pointIndex];

                    int16_t x = points.contourX[points.pointCount-1 - pointIndex];
                    int16_t y = points.contourY[points.pointCount-1 - pointIndex];
                    output.contourX[beginRange + pointIndex] = (int16_t)(m * (a*x + c*y + e));
                    output.contourY[beginRange + pointIndex] = (int16_t)(n * (b*x + d*y + f));
                }
            }
        }
    }

    return output;
}

#endif

} // namespace ttftk
