/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "MpegOutputStream.h"
#include "MpegGenerator.h"


elMpegOutputStream::elMpegOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex):
    elOutputStream(Gen, StreamIndex)
{
    return;
}

elMpegOutputStream::~elMpegOutputStream()
{
    return;
}

unsigned int elMpegOutputStream::Read(uint8_t* Buffer, unsigned int BufferSize)
{
    if (m_CurrentFrame >= m_Gen.GetFrameCount())
    {
        m_Eos = true;
        return 0;
    }
    m_Eos = false;
    return m_Gen.ReadFrame(Buffer, BufferSize, m_CurrentFrame++, m_StreamIndex);
}
