/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "SingleBlockLoader.h"
#include "../Parser.h"
#include "../Parsers/ParserVersion6.h"

elSingleBlockLoader::elSingleBlockLoader() :
    m_Compression(0),
    isSecondPart(false)
{
    return;
}

elSingleBlockLoader::~elSingleBlockLoader()
{
    return;
}

const std::string elSingleBlockLoader::GetName() const
{
    return "Single Block Header";
}

bool elSingleBlockLoader::Initialize(std::istream* Input)
{
    elBlockLoader::Initialize(Input);

    const std::streamoff StartOffset = m_Input->tellg();

    // Read in some values
    uint8_t Compression;
    uint8_t ChannelValue;
    uint16_t SampleRate;
    uint32_t TotalSamples;
    uint32_t StartingPartSamples = 0;
    uint32_t BlockSize;
    uint32_t FirstPartSamples;

    if (StartOffset != 0)
    {
        m_Input->seekg(4);
        m_Input->read((char*)&TotalSamples, 4);
        Swap(TotalSamples);
        if ((TotalSamples & 0x20000000) != 0)
        {
            // The track is a loop, with a starting part and a main part
            m_Input->read((char*)&StartingPartSamples, 4);
            Swap(StartingPartSamples);
            TotalSamples = (TotalSamples & 0x1FFFFFFF);
            uint32_t MainPartSamples;
            m_Input->seekg(StartOffset + 4);
            m_Input->read((char*)&MainPartSamples, 4);
            Swap(MainPartSamples);
            if (StartingPartSamples + MainPartSamples == TotalSamples)
            {
                VERBOSE("L: single block loader, main part of the loop");
                isSecondPart = true;
                m_Input->seekg(0);
                m_Input->read((char*)&Compression, 1);
                m_Compression = Compression;
                m_Input->clear();
                m_Input->seekg(StartOffset);
                return true;
            }
        }
        m_Input->clear();
        m_Input->seekg(StartOffset);
    }
    
    m_Input->read((char*)&Compression, 1);
    m_Input->read((char*)&ChannelValue, 1);
    m_Input->read((char*)&SampleRate, 2);
    m_Input->read((char*)&TotalSamples, 4);

    Swap(SampleRate);
    Swap(TotalSamples);
    if ((TotalSamples & 0x20000000) != 0)
    {
        m_Input->read((char*)&StartingPartSamples, 4);
        Swap(StartingPartSamples);
    }
    TotalSamples = (TotalSamples & 0x1FFFFFFF);

    m_Input->read((char*)&BlockSize, 4);
    m_Input->read((char*)&FirstPartSamples, 4);

    Swap(BlockSize);
    Swap(FirstPartSamples);

    // Make sure its valid
    if (Compression < 5 || Compression > 7)
    {
        VERBOSE("L: single block loader incorrect because of compression");
        return false;
    }
    m_Compression = Compression;
    
    if (ChannelValue % 4 != 0)
    {
        VERBOSE("L: single block loader incorrect because of channel value");
        return false;
    }
    if ((StartingPartSamples == 0) && (TotalSamples != FirstPartSamples))
    {
        VERBOSE("L: single block loader incorrect because total samples don't equal first (unique) part samples");
        return false;
    }
    if ((StartingPartSamples != 0) && (StartingPartSamples != FirstPartSamples))
    {
        VERBOSE("L: single block loader incorrect because loop starting part samples don't equal first part samples");
        return false;
    }
    m_Input->seekg(0, std::ios_base::end);
    if (BlockSize + 8 > m_Input->tellg())
    {
        VERBOSE("L: single block loader incorrect because of size");
        return false;
    }


    VERBOSE("L: single block loader correct");
    m_Input->clear();
    m_Input->seekg(StartOffset);
    return true;
}

bool elSingleBlockLoader::ReadNextBlock(elBlock& Block)
{
    if (m_Input->eof() || m_CurrentBlockIndex)
    {
        return false;
    }

    std::streamoff Offset = m_Input->tellg();

    // Read in some values
    uint8_t Compression;
    uint8_t ChannelValue;
    uint16_t SampleRate;
    uint32_t TotalSamples;
    uint32_t StartingPartSamples = 0;
    uint32_t BlockSize;
    uint32_t FirstPartSamples;

    if (!isSecondPart)
    {
        m_Input->read((char*)&Compression, 1);
        m_Input->read((char*)&ChannelValue, 1);
        m_Input->read((char*)&SampleRate, 2);
        m_Input->read((char*)&TotalSamples, 4);

        Swap(SampleRate);
        Swap(TotalSamples);
        if ((TotalSamples & 0x20000000) != 0)
        {
            // The track is a loop, with a starting part and a main part
            m_Input->read((char*)&StartingPartSamples, 4);
            Swap(StartingPartSamples);
        }
        TotalSamples = (TotalSamples & 0x1FFFFFFF);
    }
    m_Input->read((char*)&BlockSize, 4);
    m_Input->read((char*)&FirstPartSamples, 4);

    Swap(BlockSize);
    Swap(FirstPartSamples);

    // Now load the data
    BlockSize -= 8;

    shared_array<uint8_t> Data(new uint8_t[BlockSize]);
    m_Input->read((char*)Data.get(), BlockSize);

    Block.Clear();
    Block.Data = Data;
    Block.SampleCount = FirstPartSamples;
    Block.Size = BlockSize;
    Block.Offset = Offset;

    m_CurrentBlockIndex++;
    return true;
}

shared_ptr<elParser> elSingleBlockLoader::CreateParser() const
{
    switch (m_Compression)
    {
        case 5:
            return make_shared<elParser>();
        case 6:
        case 7:
            return make_shared<elParserVersion6>();
    }
    return shared_ptr<elParser>();
}

void elSingleBlockLoader::ListSupportedParsers(std::vector< std::string >& Names) const
{
    Names.push_back(make_shared<elParser>()->GetName());
    Names.push_back(make_shared<elParserVersion6>()->GetName());
    return;
}
