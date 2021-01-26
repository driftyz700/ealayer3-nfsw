/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "ParserForSCx.h"
#include "../Bitstream.h"

elParserForSCx::elParserForSCx()
{
    return;
}

elParserForSCx::~elParserForSCx()
{
    return;
}

const std::string elParserForSCx::GetName() const
{
    return "EAL3 for SCx blocks";
}

bool elParserForSCx::ReadGranuleWithUncSamples(bsBitstream& IS, elGranule& Gr)
{
    if (IS.Eos())
    {
        return false;
    }

    if (IS.GetCountBitsLeft() < 32)
    {
        return false;
    }

    // We will check for uncompressed samples at the end
    if (IS.ReadBits(8))
    {
        throw (elParserException("The first granule is uncompressed samples, don't know how to handle that."));
    }

    if (!ReadGranule(IS, Gr))
    {
        return false;
    }

    // Check if this is the last granule in the block
    if (Gr.Version == 0 && Gr.SampleRateIndex == 0 && Gr.ChannelMode == 0 &&
        Gr.ModeExtension == 0 && Gr.Index == 0)
    {
        VERBOSE("P: " << GetName() << " null granule encountered, end of stream");
        return false;
    }

    // See if there are any uncompressed samples
    IS.SeekToNextByte();
    if (!IS.Eos())
    {
        int UncompressedSamples = IS.ReadBits(8);

        if (UncompressedSamples)
        {
            unsigned int Unknown = IS.ReadAligned16BE<unsigned int>();
            Gr.Uncomp.Count = IS.ReadAligned16BE<unsigned int>();
            VERBOSE("  Unknown: " << Unknown << ", Count: " << Gr.Uncomp.Count << ", Granule: " << (int)Gr.Index);
            //Gr.Uncomp.OffsetInOutput = Unknown - Gr.Uncomp.Count;
            ReadUncSamples(IS, Gr);
        }
        else
        {
            Gr.Uncomp.Count = 0;
            Gr.Uncomp.OffsetInOutput = 0;
            IS.SeekRelative(-8);
        }
    }
    else
    {
        Gr.Uncomp.Count = 0;
        Gr.Uncomp.OffsetInOutput = 0;
    }

    Gr.Used = true;
    return true;
}
