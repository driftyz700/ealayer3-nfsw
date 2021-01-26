/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "AsfPtLoader.h"

elAsfPtLoader::elAsfPtLoader() :
    m_BlockCount(0)
{
    return;
}

elAsfPtLoader::~elAsfPtLoader()
{
    return;
}

const std::string elAsfPtLoader::GetName() const
{
    return "Asf PT Header";
}

bool elAsfPtLoader::Initialize(std::istream* Input)
{
    elBlockLoader::Initialize(Input);

    char Signature[4];
    unsigned int BlockSize;
    shared_array<uint8_t> Data;

    // Read in the header
    Data = ReadRawBlockFromInput(Signature, BlockSize);
    if (memcmp(Signature, "SCHl", 4) != 0)
    {
        return false;
    }

    uint8_t* Ptr = Data.get();
    if (memcmp(Ptr, "PT", 2) != 0)
    {
        return false;
    }

    // Set it up
    Ptr += 4;
    BlockSize -= 4;
    m_BlockCount = 0;

    // Read the header
    if (!ParseVariableHeader(Ptr, BlockSize))
    {
        return false;
    }

    // Check it
    if (!m_Split || m_SplitCompression != 0x17)
    {
        return false;
    }

    // Read in the number of blocks
    Data = ReadRawBlockFromInput(Signature, BlockSize);
    if (memcmp(Signature, "SCCl", 4) != 0)
    {
        return false;
    }

    m_BlockCount = *(uint32_t*)Data.get();
    Swap(m_BlockCount);

    // Next block should be the data
    return true;
}
