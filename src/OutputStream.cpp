/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "OutputStream.h"
#include "MpegGenerator.h"

elOutputStream::elOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex) :
    m_Gen(Gen),
    m_StreamIndex(StreamIndex),
    m_CurrentFrame(0),
    m_Eos(false)
{
    return;
}

elOutputStream::~elOutputStream()
{
    return;
}

unsigned int elOutputStream::GetSampleRate() const
{
    return m_Gen.GetSampleRate(m_StreamIndex);
}

unsigned int elOutputStream::GetChannels() const
{
    return m_Gen.GetChannels(m_StreamIndex);
}

bool elOutputStream::Eos() const
{
    return m_Eos;
}
