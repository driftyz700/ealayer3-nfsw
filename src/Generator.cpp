/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010-2011, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "Generator.h"
#include "Parser.h"

#include "Bitstream.h"
#include "BlockLoader.h"

elGenerator::elGenerator()
{
    return;
}

elGenerator::~elGenerator()
{
    return;
}

void elGenerator::Initialize()
{
    m_Streams.clear();
    return;
}

void elGenerator::AddFrameFromStream(const elFrame& Fr)
{
    m_Streams.push_back(Fr);
    return;
}

void elGenerator::Clear()
{
    m_Streams.clear();
    return;
}


bool elGenerator::Generate(elBlock& Block, bool first, unsigned int Keep)
{
    // If we don't have any streams return false
    if (m_Streams.size() < 1)
    {
        return false;
    }
    
    // This is probably a bad assumption, but assume that there is enough
    // space for our block if the block is allocated at all.
    if (Keep == 0)
    {
        if (!Block.Data)
        {
            Block.Data = shared_array<uint8_t>(new uint8_t[2880 * 8]);
        }
        Block.Flags = 0;
        if (first) {
            Block.SampleCount = ENCODER_UNCOM_SAMPLES;
        }
        else {
            Block.SampleCount = 1152; // TODO Fix me!!!
        }
        Block.Size = 0;
        Block.Offset = 0;

    }
    else
    {
    }

    // The output streams
    bsBitstream OS(Block.Data.get(), 2880 * 8);
    
    // Loop through each granule
    for (unsigned int i = 0; i < 2; i++)
    {
        // Loop through the streams
        for (std::vector<elFrame>::const_iterator FrIter = m_Streams.begin();
            FrIter != m_Streams.end(); ++FrIter)
        {
            const elGranule& Gr = FrIter->Gr[i];
            if (Gr.Used)
            {
                WriteGranuleWithUncSamples(OS, Gr);

                // Set the block properties
                Block.SampleRate = Gr.SampleRate;
                Block.Channels = Gr.Channels;
            }
        }
    }

    // Finalize the block
    OS.WriteToNextByte();
    Block.Size = OS.Tell() / 8;

    // Clear the streams
    m_Streams.clear();
    return true;
}

void elGenerator::WriteGranuleWithUncSamples(bsBitstream& OS, const elGranule& Gr)
{
    // Are there uncompressed samples?
    OS.WriteBits(Gr.Uncomp.Count ? 0xEE : 0x00, 8);

    // Write the compressed part
    WriteGranule(OS, Gr);
    OS.WriteToNextByte();

    // Write the uncompressed samples
    if (Gr.Uncomp.Count)
    {
        OS.WriteAligned32BE<unsigned int>(Gr.Uncomp.Count);
        OS.WriteAligned32BE<unsigned int>(Gr.Uncomp.OffsetInOutput);
        WriteUncSamples(OS, Gr);
    }
    return;
}

void elGenerator::WriteGranule(bsBitstream& OS, const elGranule& Gr)
{
    // Write some fields out
    OS.WriteBits(Gr.Version, 2);
    OS.WriteBits(Gr.SampleRateIndex, 2);
    OS.WriteBits(Gr.ChannelMode, 2);
    OS.WriteBits(Gr.ModeExtension, 2);
    OS.WriteBit(Gr.Index);

    // Write out scfsi
    if (Gr.Index == 1 && Gr.Version == MV_1)
    {
        for (unsigned int i = 0; i < Gr.Channels; i++)
        {
            OS.WriteBits(Gr.ChannelInfo[i].Scfsi, 4);
        }
    }

    // Write out the side info
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        OS.WriteBits(Gr.ChannelInfo[i].Size, 12);
        OS.WriteBits(Gr.ChannelInfo[i].SideInfo[0], 32);
        if (Gr.Version == MV_1)
        {
            OS.WriteBits(Gr.ChannelInfo[i].SideInfo[1], 47 - 32);
        }
        else
        {
            OS.WriteBits(Gr.ChannelInfo[i].SideInfo[1], 51 - 32);
        }
    }

    // Write out the data
    unsigned long BitsLeft = Gr.DataSizeBits;
    if (BitsLeft > 0)
    {
        bsBitstream IS(Gr.Data.get(), BitsLeft);

        while (BitsLeft)
        {
            unsigned int ToWrite = min(32, BitsLeft);
            OS.WriteBits(IS.ReadBits(ToWrite), ToWrite);
            BitsLeft -= ToWrite;
        }
    }
    return;
}

void elGenerator::WriteUncSamples(bsBitstream& OS, const elGranule& Gr)
{
	// Write out the samples, interleaving them
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        for (unsigned int j = 0; j < Gr.Uncomp.Count; j++)
        {
            OS.WriteAligned16BE<short>(Gr.Uncomp.Data[j * Gr.Channels + i]);
        }
    }
    return;
}
