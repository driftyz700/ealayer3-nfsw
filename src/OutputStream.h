/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"

class elMpegGenerator;

class elOutputStream
{
public:
    elOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex);
    virtual ~elOutputStream();

    /// Get the sample rate of this stream.
    virtual unsigned int GetSampleRate() const;

    /// Get the number of channels in this stream.
    virtual unsigned int GetChannels() const;

    /// Are we at the end of the stream?
    virtual bool Eos() const;
    
protected:
    const elMpegGenerator& m_Gen;
    unsigned int m_StreamIndex;
    unsigned int m_CurrentFrame;
    bool m_Eos;
};
