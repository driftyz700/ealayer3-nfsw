/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "../BlockLoader.h"

class elSCxLoader : public elBlockLoader
{
public:
    elSCxLoader();
    virtual ~elSCxLoader();

    /// Reads the next block from the file and updates the current block index.
    virtual bool ReadNextBlock(elBlock& Block);
    
    /// Creates an EALayer3 parser for this particular file.
    virtual shared_ptr<elParser> CreateParser() const;
    
    /// Adds the names of the supported parsers to List.
    virtual void ListSupportedParsers(std::vector<std::string>& Names) const;

protected:
    /// Read the next raw EA block from the input file.
    shared_array<uint8_t> ReadRawBlockFromInput(char* Type, unsigned int& Size);

    /// Parse a variable-length field header, PT or GSTR.
    bool ParseVariableHeader(uint8_t* Ptr, unsigned int Size);

    /// Clear the header fields (below).
    void ClearHeaderFields();

    unsigned int m_Channels;
    unsigned int m_Compression;
    unsigned int m_SampleRate;
    unsigned int m_SampleCount;
    unsigned int m_LoopOffset;
    unsigned int m_LoopLength;
    unsigned int m_DataStart;
    unsigned int m_BytesPerSample;
    unsigned int m_Split;
    unsigned int m_SplitCompression;
};
