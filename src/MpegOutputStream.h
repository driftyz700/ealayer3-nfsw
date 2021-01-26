/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "OutputStream.h"

class elMpegGenerator;

class elMpegOutputStream : public elOutputStream
{
public:
    elMpegOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex);
    virtual ~elMpegOutputStream();

    /// Read an MPEG frame from the stream.
    virtual unsigned int Read(uint8_t* Buffer, unsigned int BufferSize);
};
