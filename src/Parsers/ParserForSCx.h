/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "../Parser.h"

/// The EALayer3 parser class for ASF files.
class elParserForSCx : public elParser
{
public:
    elParserForSCx();
    virtual ~elParserForSCx();

    /// Get the name associated with this parser.
    virtual const std::string GetName() const;

protected:
    /// Read a granule and uncompressed samples if existant from the stream.
    virtual bool ReadGranuleWithUncSamples(bsBitstream& IS, elGranule& Gr);
};
