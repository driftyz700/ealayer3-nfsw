/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "HeaderlessWriter.h"

elHeaderlessWriter::elHeaderlessWriter()
{
    return;
}

elHeaderlessWriter::~elHeaderlessWriter()
{
    return;
}

void elHeaderlessWriter::Initialize(std::ostream* Output)
{
    if (!Output)
    {
        return;
    }
    elBlockWriter::Initialize(Output);

    // No header. Otherwise it would be written here.
    return;
}

void elHeaderlessWriter::WriteNextBlock(const elBlock& Block, bool LastBlock)
{
    // Sanity check
    if (!m_Output)
    {
        return;
    }

    // Calculate the variables
    uint16_t Flags;
    uint16_t BlockSize;
    uint32_t Samples;

    Flags = LastBlock ? 0x8000 : 0x0000;
    BlockSize = Block.Size + 8;
    Samples = Block.SampleCount;

    // Swap
    Swap(Flags);
    Swap(BlockSize);
    Swap(Samples);

    // Write
    m_Output->write((char*)&Flags, 2);
    m_Output->write((char*)&BlockSize, 2);
    m_Output->write((char*)&Samples, 4);
    m_Output->write((char*)Block.Data.get(), Block.Size);
    return;
}
