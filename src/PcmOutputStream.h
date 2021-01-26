/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "OutputStream.h"

class elMpegGenerator;
struct mpg123_handle_struct;
typedef struct mpg123_handle_struct mpg123_handle;

class elPcmOutputStream : public elOutputStream
{
public:
    elPcmOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex);
    virtual ~elPcmOutputStream();

    /// Read PCM samples from the stream.
    virtual unsigned int Read(short* Buffer, unsigned int BufferSamples);

    /// Get the size of the buffer that should be used.
    static unsigned int RecommendBufferSize();

protected:
    /// Feed the next frame into the decoder.
    unsigned int FeedNextFrame();
    
    /// Add the uncompressed samples to the frame.
    unsigned int FixupOutFrame(short* Buffer, unsigned int BufferSamples, unsigned int FrameIndex);

    mpg123_handle* m_Decoder;
    unsigned long m_SamplesLeft;
};

class elMpg123Exception : public std::exception
{
public:
    elMpg123Exception(int ErrorCode) throw();
    virtual ~elMpg123Exception() throw();
    virtual const char* what() const throw();

protected:
    int m_ErrorCode;
};
