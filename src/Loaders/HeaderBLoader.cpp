/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "HeaderBLoader.h"
#include "../Parsers/ParserVersion6.h"

elHeaderBLoader::elHeaderBLoader() :
    m_SampleRate(0),
    m_UseParser6(true)
{
    return;
}

elHeaderBLoader::~elHeaderBLoader()
{
    return;
}

const std::string elHeaderBLoader::GetName() const
{
    return "Header B";
}

bool elHeaderBLoader::Initialize(std::istream* Input)
{
    elBlockLoader::Initialize(Input);

    // Read the small header
    uint16_t BlockType;
    uint16_t BlockSize;

    m_Input->read((char*)&BlockType, 2);
    m_Input->read((char*)&BlockSize, 2);
    Swap(BlockType);
    Swap(BlockSize);

    if (BlockType != 0x4800)
    {
        VERBOSE("L: header B loader incorrect because of block type");
        return false;
    }
    if (BlockSize < 8)
    {
        VERBOSE("L: header B loader incorrect because of block size");
        return false;
    }

    // Read in the contents of the block
    uint8_t Compression;
    uint16_t SampleRate;

    m_Input->read((char*)&Compression, 1);
    m_Input->get();
    m_Input->read((char*)&SampleRate, 2);
    Swap(SampleRate);

    // Different parsers for different values
    if (Compression == 0x15)
    {
        m_UseParser6 = false;
    }
    else if (Compression == 0x16)
    {
        m_UseParser6 = true;
    }
    else
    {
        VERBOSE("L: header B loader incorrect because of compression");
        return false;
    }
    m_SampleRate = SampleRate;

    m_Input->seekg(BlockSize - 8, std::ios_base::cur);
    VERBOSE("L: header B loader correct");
    return true;
}

bool elHeaderBLoader::ReadNextBlock(elBlock& Block)
{
    if (!m_Input)
    {
        return false;
    }

    if (m_Input->eof())
    {
        return false;
    }
    
    uint16_t BlockType;
    uint16_t BlockSize;
    uint32_t Samples;

    const std::streamoff Offset = m_Input->tellg();

    m_Input->read((char*)&BlockType, 2);
    m_Input->read((char*)&BlockSize, 2);
    m_Input->read((char*)&Samples, 4);

    if (m_Input->eof())
    {
        return false;
    }

    Swap(BlockType);
    Swap(BlockSize);
    Swap(Samples);

    if (BlockType == 0x4500)
    {
        return false;
    }
    else if (BlockType != 0x4400)
    {
        VERBOSE("L: header B invalid block type");
        return false;
    }

    if (BlockSize <= 8)
    {
        VERBOSE("L: header B block too small");
        return false;
    }

    BlockSize -= 8;

    shared_array<uint8_t> Data(new uint8_t[BlockSize]);
    m_Input->read((char*)Data.get(), BlockSize);

    Block.Clear();
    Block.Data = Data;
    Block.SampleCount = Samples;
    Block.Size = BlockSize;
    Block.Offset = Offset;

    m_CurrentBlockIndex++;
    return true;
}

shared_ptr<elParser> elHeaderBLoader::CreateParser() const
{
    if (m_UseParser6)
    {
        return make_shared<elParserVersion6>();
    }
    return make_shared<elParser>();
}

void elHeaderBLoader::ListSupportedParsers(std::vector<std::string>& Names) const
{
    Names.push_back(make_shared<elParser>()->GetName());
    Names.push_back(make_shared<elParserVersion6>()->GetName());
    return;
}
