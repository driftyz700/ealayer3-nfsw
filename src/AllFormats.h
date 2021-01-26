/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "FormatSelector.h"

// Include files for the types of selectors
#include "BlockLoader.h"
#include "Parser.h"


/// The block loader selector class
class elBlockLoaderSelector : public fsFormatSelector<elBlockLoader>
{
public:
    elBlockLoaderSelector();
    virtual ~elBlockLoaderSelector();

    /// Initializes the loader, returning false if this file cannot be read by this loader.
    virtual bool Initialize(std::istream* Input);

    /// Get the name associated with this loader.
    virtual const std::string GetName() const;

    /// Reads the next block from the file and updates the current block index.
    virtual bool ReadNextBlock(elBlock& Block);

    /// Gets the current block index.
    virtual unsigned int GetCurrentBlockIndex();

    /// Creates an EALayer3 parser for this particular file.
    virtual shared_ptr<elParser> CreateParser() const;

    /// Adds the names of the supported parsers to List.
    virtual void ListSupportedParsers(std::vector<std::string>& Names) const;
};

/// The EALayer3 parser selector class.
class elParserSelector : public fsFormatSelector<elParser>
{
public:
    elParserSelector();
    virtual ~elParserSelector();

    /// Parses the entire input stream and checks to see if it's a format that can be parsed.
    virtual bool Initialize(bsBitstream& IS);

    /// Get the name associated with this parser.
    virtual const std::string GetName() const;
    
    /// Parses the entire input stream and outputs an elStreamVector.
    virtual void Parse(elStreamVector& Streams, bsBitstream& IS);
};
