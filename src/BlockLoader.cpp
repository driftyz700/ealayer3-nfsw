/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "BlockLoader.h"
#include "Parser.h"

elBlock::elBlock() :
        Size(0),
        SampleCount(0),
        Flags(0),
        SampleRate(0),
        Channels(0)
{
    return;
}

elBlock::~elBlock()
{
    Clear();
    return;
}

void elBlock::Clear()
{
    Size = 0;
    SampleCount = 0;
    Offset = 0;
    SampleRate = 0;
    Channels = 0;
    return;
}

elBlockLoader::elBlockLoader() :
        m_Input(NULL),
        m_CurrentBlockIndex(0)
{
    return;
}

elBlockLoader::~elBlockLoader()
{
    return;
}

bool elBlockLoader::Initialize(std::istream* Input)
{
    if (!Input)
    {
        throw (std::exception());
    }
    m_Input = Input;
    return true;
}

unsigned int elBlockLoader::GetCurrentBlockIndex()
{
    return m_CurrentBlockIndex;
}

shared_ptr<elParser> elBlockLoader::CreateParser() const
{
    return make_shared<elParser>();
}

void elBlockLoader::ListSupportedParsers(std::vector<std::string>& Names) const
{
    return;
}
