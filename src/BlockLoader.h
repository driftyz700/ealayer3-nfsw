/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"

class elBlock
{
public:
    elBlock();
    ~elBlock();
    void Clear();

    shared_array<uint8_t> Data;
    unsigned int Size;
    unsigned int SampleCount;
    unsigned int Flags;
    std::streamoff Offset;

    unsigned int SampleRate;
    unsigned int Channels;
};

class elParser;

class elBlockLoader
{
public:
    elBlockLoader();
    virtual ~elBlockLoader();

    /// Get the name associated with this loader.
    virtual const std::string GetName() const = 0;

    /// Initializes the loader, returning false if this file cannot be read by this loader.
    virtual bool Initialize(std::istream* Input);

    /// Reads the next block from the file and updates the current block index.
    virtual bool ReadNextBlock(elBlock& Block) = 0;

    /// Gets the current block index.
    virtual unsigned int GetCurrentBlockIndex();

    /// Creates an EALayer3 parser for this particular file.
    virtual shared_ptr<elParser> CreateParser() const;

    /// Adds the names of the supported parsers to List.
    virtual void ListSupportedParsers(std::vector<std::string>& Names) const;

protected:
    std::istream* m_Input;
    unsigned int m_CurrentBlockIndex;
};


inline void Swap(uint16_t& Value)
{
    Value = (Value & 0xFF00) >> 8 | (Value & 0x00FF) << 8;
    return;
}

inline void Swap(uint32_t& Value)
{
    Value = (Value & 0xFF000000) >> 24 | (Value & 0x00FF0000) >> 8 |
        (Value & 0x000000FF) << 24 | (Value & 0x0000FF00) << 8;
    return;
}
