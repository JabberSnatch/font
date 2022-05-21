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

    if (!(subtable.scalerType == 0x00010000 // Windows/Adobe ttf
          || subtable.scalerType == 0x74727565)) // "true", OSX/iOS ttf
        return Result::UnknownScalerType;

    ttfFile.tableDirectory.resize(ttfFile.offsets.numTables);
    ptr = ExtractTableDirectory(ptr, ttfFile.tableDirectory.data(), ttfFile.offsets.numTables);

    for (uint32_t index = 0u; index < ttfFile.offsets.numTables; ++index)
    {
        TableDirectoryEntry const& entry = tableDirectory[index];

        if (CompareTagU32(entry.tag, "cmap"))
            required.cmap = &entry;
        else if (CompareTagU32(entry.tag, "glyf"))
            required.glyf = &entry;
        else if (CompareTagU32(entry.tag, "head"))
            required.head = &entry;
        else if (CompareTagU32(entry.tag, "hhea"))
            required.hhea = &entry;
        else if (CompareTagU32(entry.tag, "hmtx"))
            required.hmtx = &entry;
        else if (CompareTagU32(entry.tag, "loca"))
            required.loca = &entry;
        else if (CompareTagU32(entry.tag, "maxp"))
            required.maxp = &entry;
        else if (CompareTagU32(entry.tag, "name"))
            required.name = &entry;
        else if (CompareTagU32(entry.tag, "post"))
            required.post = &entry;

        else if (CompareTagU32(entry.tag, "cvt "))
            optional.cvt = &entry;
        else if (CompareTagU32(entry.tag, "fpgm"))
            optional.fpgm = &entry;
        else if (CompareTagU32(entry.tag, "hdmx"))
            optional.hdmx = &entry;
        else if (CompareTagU32(entry.tag, "kern"))
            optional.kern = &entry;
        else if (CompareTagU32(entry.tag, "OS/2"))
            optional.os2 = &entry;
        else if (CompareTagU32(entry.tag, "prep"))
            optional.prep = &entry;
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

            if (platformID == 0u
                && platformSpecificID != 14)
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

        if (format != 4)
            return Result::UnknownCMAPFormat;
    }

    std::swap(ttfFile, *_ttfFile);
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

#endif

} // namespace ttftk
