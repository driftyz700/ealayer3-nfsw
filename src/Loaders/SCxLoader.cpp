/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "SCxLoader.h"
#include "../Parsers/ParserForSCx.h"

elSCxLoader::elSCxLoader()
{
    ClearHeaderFields();
    return;
}

elSCxLoader::~elSCxLoader()
{
    return;
}

bool elSCxLoader::ReadNextBlock(elBlock& Block)
{
    if (!m_Input)
    {
        return false;
    }
    if (m_Input->eof())
    {
        return false;
    }

    const std::streamoff Offset = m_Input->tellg();

    // Read the block
    char Signature[4];
    unsigned int BlockSize;
    shared_array<uint8_t> Data;

    Data = ReadRawBlockFromInput(Signature, BlockSize);
    if (memcmp(Signature, "SCEl", 4) == 0)
    {
        return false;
    }
    if (memcmp(Signature, "SCDl", 4) != 0)
    {
        return ReadNextBlock(Block);
    }
    if (BlockSize < 12)
    {
        return false;
    }

    // Get some vars
    uint8_t* Ptr = Data.get();
    unsigned int SampleFrames;
    unsigned int Null;
    unsigned int Unknown;

    BlockSize -= 12;
    SampleFrames = *(uint32_t*)Ptr;
    Ptr += 4;
    Null = *(uint32_t*)Ptr;
    Ptr += 4;
    Unknown = *(uint32_t*)Ptr;
    Ptr += 4;

    Swap(SampleFrames);
    Swap(Null);
    Swap(Unknown);

    // Set up the block
    Block.Clear();
    Block.Offset = Offset;
    Block.SampleCount = SampleFrames;
    Block.Size = BlockSize;
    Block.Data = shared_array<uint8_t>(new uint8_t[BlockSize]);
    memcpy(Block.Data.get(), Ptr, BlockSize);
    return true;
}

shared_ptr<elParser> elSCxLoader::CreateParser() const
{
    return make_shared<elParserForSCx>();
}

void elSCxLoader::ListSupportedParsers(std::vector< std::string >& Names) const
{
    Names.push_back(make_shared<elParserForSCx>()->GetName());
    return;
}

shared_array<uint8_t> elSCxLoader::ReadRawBlockFromInput(char* Type, unsigned int& Size)
{
    if (!m_Input)
    {
        return shared_array<uint8_t>();
    }

    m_Input->read(Type, 4);
    m_Input->read((char*)&Size, 4);

    if (Size <= 8)
    {
        return shared_array<uint8_t>();
    }
    Size -= 8;

    const std::streamoff CurrentOffset = m_Input->tellg();
    m_Input->seekg(0, std::ios_base::end);
    const std::streamsize FileSize = m_Input->tellg();
    m_Input->seekg(CurrentOffset);

    if (Size > FileSize - CurrentOffset)
    {
        Size = 0;
        return shared_array<uint8_t>();
    }

    shared_array<uint8_t> Data(new uint8_t[Size]);
    m_Input->read((char*)Data.get(), Size);
    return Data;
}

static unsigned long ReadBytes(uint8_t*& Ptr, uint8_t Count)
{
    uint8_t i;
    uint8_t Byte;
    unsigned long Result;

    Result = 0;
    for (i = 0; i < Count; i++)
    {
        Byte = *Ptr++;
        Result <<= 8;
        Result |= Byte;
    }
    return Result;
}

bool elSCxLoader::ParseVariableHeader(uint8_t* Ptr, unsigned int Size)
{
    ClearHeaderFields();

    // Some vars
    const uint8_t* StartPtr = Ptr;
    uint8_t Byte;
    bool InHeader;
    bool InSubHeader;

    InHeader = true;
    while (InHeader && (Ptr - StartPtr) < Size)
    {
        Byte = *Ptr++;
        switch (Byte) // parse header code
        {
        case 0xFF: // end of header
            InHeader = false;
        case 0xFE: // skip
        case 0xFC: // skip
            break;
        case 0xFD: // subheader starts...
            InSubHeader = true;
            while (InSubHeader && (Ptr - StartPtr) < Size)
            {
                Byte = *Ptr++;
                switch (Byte) // parse subheader code
                {
                case 0x80:
                    Byte = *Ptr++;
                    m_Split = ReadBytes(Ptr, Byte);
                    break;
                case 0x82:
                    Byte = *Ptr++;
                    m_Channels = ReadBytes(Ptr, Byte);
                    break;
                case 0x83:
                    Byte = *Ptr++;
                    m_Compression = ReadBytes(Ptr, Byte);
                    break;
                case 0x84:
                    Byte = *Ptr++;
                    m_SampleRate = ReadBytes(Ptr, Byte);
                    break;
                case 0x85:
                    Byte = *Ptr++;
                    m_SampleCount = ReadBytes(Ptr, Byte);
                    break;
                case 0x86:
                    Byte = *Ptr++;
                    m_LoopOffset = ReadBytes(Ptr, Byte);
                    break;
                case 0x87:
                    Byte = *Ptr++;
                    m_LoopLength = ReadBytes(Ptr, Byte);
                    break;
                case 0x88:
                    Byte = *Ptr++;
                    m_DataStart = ReadBytes(Ptr, Byte);
                    break;
                case 0x92:
                    Byte = *Ptr++;
                    m_BytesPerSample = ReadBytes(Ptr, Byte);
                    break;
                case 0xA0:
                    Byte = *Ptr++;
                    m_SplitCompression = ReadBytes(Ptr, Byte);
                    break;
                case 0xFF:
                    InHeader = false;
                    InSubHeader = false;
                    break;
                case 0x8A: // end of subheader
                    InSubHeader = false;
                default: // ???
                    Byte = *Ptr++;
                    Ptr += Byte;
                }
            }
            break;
        default:
            Byte = *Ptr++;
            if (Byte == 0xFF)
            {
                Ptr += 4;
            }
            Ptr += Byte;
        }
    }
    return true;
}

void elSCxLoader::ClearHeaderFields()
{
    m_Channels = 0;
    m_Compression = 0;
    m_SampleRate = 0;
    m_SampleCount = 0;
    m_LoopOffset = 0;
    m_LoopLength = 0;
    m_DataStart = 0;
    m_BytesPerSample = 0;
    m_Split = 0;
    m_SplitCompression = 0;
    return;
}
