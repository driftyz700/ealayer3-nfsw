/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "SCxLoader.h"

class elAsfPtLoader : public elSCxLoader
{
public:
    elAsfPtLoader();
    virtual ~elAsfPtLoader();

    /// Get the name associated with this loader.
    virtual const std::string GetName() const;

    /// Initializes the loader, returning false if this file cannot be read by this loader.
    virtual bool Initialize(std::istream* Input);

protected:
    unsigned int m_BlockCount;
};
