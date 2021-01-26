/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "HeaderBWriter.h"

elHeaderBWriter::elHeaderBWriter() :
    m_WrittenHeader(false)
{
    return;
}

elHeaderBWriter::~elHeaderBWriter()
{
    return;
}

void elHeaderBWriter::Initialize(std::ostream* Output)
{
    if (!Output)
    {
        return;
    }
    elBlockWriter::Initialize(Output);
    
    m_WrittenHeader = false;
    return;
}

void elHeaderBWriter::WriteNextBlock(const elBlock& Block, bool LastBlock)
{
    // Sanity check
    if (!m_Output)
    {
        return;
    }

    if (!m_WrittenHeader)
    {
        WriteHeader(Block);
        m_WrittenHeader = true;
    }

    // Calculate the variables
    uint16_t BlockType = 0x4400;
    uint16_t BlockSize;
    uint32_t Samples;

    BlockSize = Block.Size + 8;
    Samples = Block.SampleCount;

    // Swap
    Swap(BlockType);
    Swap(BlockSize);
    Swap(Samples);

    // Write
    m_Output->write((char*)&BlockType, 2);
    m_Output->write((char*)&BlockSize, 2);
    m_Output->write((char*)&Samples, 4);
    m_Output->write((char*)Block.Data.get(), Block.Size);

    if (LastBlock)
    {
        WriteEnder();
    }
    return;
}

void elHeaderBWriter::WriteHeader(const elBlock& Block)
{
    uint16_t BlockType = 0x4800;
    uint16_t BlockSize = 0x0C;

    Swap(BlockType);
    Swap(BlockSize);

    m_Output->write((char*)&BlockType, 2);
    m_Output->write((char*)&BlockSize, 2);

    // Read in the contents of the block
    uint8_t Compression = 0x15;
    uint8_t ChannelValue = 4 * (Block.Channels - 1);
    uint16_t SampleRate = Block.SampleRate;
    uint32_t Unknown = 0x40000000;

    Swap(SampleRate);
    Swap(Unknown);

    m_Output->write((char*)&Compression, 1);
    m_Output->write((char*)&ChannelValue, 1);
    m_Output->write((char*)&SampleRate, 2);
    m_Output->write((char*)&Unknown, 4);
    return;
}

void elHeaderBWriter::WriteEnder()
{
    uint16_t BlockType = 0x4500;
    uint16_t BlockSize = 0x04;

    Swap(BlockType);
    Swap(BlockSize);

    m_Output->write((char*)&BlockType, 2);
    m_Output->write((char*)&BlockSize, 2);
    return;
}
