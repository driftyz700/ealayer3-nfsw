/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "../Parser.h"

/// The EALayer3 parser class for version 6 and 7 (CHECK) files.
class elParserVersion6 : public elParser
{
public:
    elParserVersion6();
    virtual ~elParserVersion6();

    /// Get the name associated with this parser.
    virtual const std::string GetName() const;

protected:
    /// Read a granule and uncompressed samples if existant from the stream.
    virtual bool ReadGranuleWithUncSamples(bsBitstream& IS, elGranule& Gr);
};
