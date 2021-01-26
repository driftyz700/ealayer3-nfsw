/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "../BlockLoader.h"

class elSingleBlockLoader : public elBlockLoader
{
public:
    elSingleBlockLoader();
    virtual ~elSingleBlockLoader();

    /// Get the name associated with this loader.
    virtual const std::string GetName() const;

    /// Initializes the loader, returning false if this file cannot be read by this loader.
    virtual bool Initialize(std::istream* Input);

    /// Reads the next block from the file and updates the current block index.
    virtual bool ReadNextBlock(elBlock& Block);

    /// Creates an EALayer3 parser for this particular file.
    virtual shared_ptr<elParser> CreateParser() const;

    /// Adds the names of the supported parsers to List.
    virtual void ListSupportedParsers(std::vector<std::string>& Names) const;

protected:
    unsigned int m_Compression;
    bool isSecondPart;
};
