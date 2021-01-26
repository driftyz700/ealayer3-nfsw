/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "AllFormats.h"
#include "Bitstream.h"

// Include files for the formats go here
#include "Loaders/HeaderlessLoader.h"
#include "Loaders/SingleBlockLoader.h"
#include "Loaders/AsfGstrLoader.h"
#include "Loaders/AsfPtLoader.h"
#include "Loaders/HeaderBLoader.h"


elBlockLoaderSelector::elBlockLoaderSelector()
{
    fsFormat Formats[] = {
        make_shared<elAsfGstrLoader>(),
        make_shared<elAsfPtLoader>(),
        make_shared<elSingleBlockLoader>(),
        make_shared<elHeaderBLoader>(),
        make_shared<elHeaderlessLoader>()
    };
    SelectorListAdd(Formats, sizeof(Formats) / sizeof(fsFormat));
    return;
}

elBlockLoaderSelector::~elBlockLoaderSelector()
{
    return;
}

bool elBlockLoaderSelector::Initialize(std::istream* Input)
{
    if (!Input)
    {
        return false;
    }

    const std::streamoff StartOffset = Input->tellg();

    // Go through each of the items in the list and look for one that can load this
    for (fsFormatList::iterator Iter = SelectorList().begin();
        Iter != SelectorList().end(); ++Iter)
    {
        Input->clear();
        Input->seekg(StartOffset);
        if ((*Iter)->Initialize(Input))
        {
            SetSelectorUsed(*Iter);
            return true;
        }
    }
    return false;
}

const std::string elBlockLoaderSelector::GetName() const
{
    return SU()->GetName();
}

bool elBlockLoaderSelector::ReadNextBlock(elBlock& Block)
{
    return SU()->ReadNextBlock(Block);
}

unsigned int elBlockLoaderSelector::GetCurrentBlockIndex()
{
    return SU()->GetCurrentBlockIndex();
}

shared_ptr<elParser> elBlockLoaderSelector::CreateParser() const
{
    return SU()->CreateParser();
}

void elBlockLoaderSelector::ListSupportedParsers(std::vector<std::string>& Names) const
{
    return SU()->ListSupportedParsers(Names);
}

elParserSelector::elParserSelector()
{
    // No need to add the formats -- they'll be added in elBlockLoader::CreateParser()
    return;
}

elParserSelector::~elParserSelector()
{
    return;
}

bool elParserSelector::Initialize(bsBitstream& IS)
{
    const unsigned long StartOffset = IS.Tell();

    // Go through each of the items in the list and look for one that can load this
    for (fsFormatList::iterator Iter = SelectorList().begin();
        Iter != SelectorList().end(); ++Iter)
    {
        IS.SeekAbsolute(StartOffset);
        if ((*Iter)->Initialize(IS))
        {
            SetSelectorUsed(*Iter);
            return true;
        }
    }
    return false;
}

const std::string elParserSelector::GetName() const
{
    return SU()->GetName();
}

void elParserSelector::Parse(elStreamVector& Streams, bsBitstream& IS)
{
    return SU()->Parse(Streams, IS);
}
