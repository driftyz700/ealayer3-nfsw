/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010-2011, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "MpegGenerator.h"
#include "MpegOutputStream.h"
#include "PcmOutputStream.h"
#include "BlockLoader.h"
#include "Bitstream.h"

#define VBR_FRAMES_FLAG         0x0001
#define VBR_BYTES_FLAG          0x0002
#define VBR_TOC_FLAG            0x0004
#define VBR_SCALE_FLAG          0x0008

static const char* MpegVersionString[4] = {"2.5", "reserved", "2", "1"};

static const unsigned int MpegSampleRateTable[4][4] = {
    {11025, 12000, 8000, 0},
    {0, 0, 0, 0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0}
};

static const unsigned int MpegBitrateTable[4][16] = {
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}
};

static const char* MpegChannelModeString[4] = {"stereo", "joint stereo", "dual channel", "mono"};

static const char* MpegModeExtensionString[4] = {"none", "intensity stereo", "MS stereo", "intensity stereo and MS stereo"};


elMpegGenerator::elMpegGenerator() :
        m_CurrentFrame(0),
        m_UncompressedSampleFrames(0),
        m_SampleFrames(0),
        m_DoneParsingBlocks(false),
        m_CurMpegFrame(0),
        m_CurOutputMpegFrame(0)
{
    return;
}

elMpegGenerator::~elMpegGenerator()
{
    return;
}

void elMpegGenerator::Clear()
{
    m_Parser.reset();
    m_UncompressedSampleFrames = 0;
    m_StreamInfo.clear();
    m_DoneParsingBlocks = false;
    m_CurMpegFrame = 0;
    m_CurOutputMpegFrame = 0;
    m_Outputs.clear();
    return;
}

bool elMpegGenerator::Initialize(const elBlock& FirstBlock, shared_ptr<elParser> Parser)
{
    Clear();

    // Set up the parser
    m_Parser = Parser;
    if (!m_Parser)
    {
        return false;
    }

    // Create the input stream
    bsBitstream IS(FirstBlock.Data.get(), FirstBlock.Size);
    elStreamVector Streams;
    
    if(!m_Parser->Initialize(IS))
    {
        return false;
    }

    IS.SeekAbsolute(0);
    ReadBlockData(Streams, IS);

    // Create a frame for each stream
    for (unsigned int i = 0; i < Streams.size(); i++)
    {
        if (Streams[i].size() < 1)
        {
            return false;
        }
        
        // Add the stream
        elStreamInfo MpegStream;
        MpegStream.Channels = Streams[i][0].Gr[0].Channels;
        MpegStream.SampleRate = Streams[i][0].Gr[0].SampleRate;
        m_StreamInfo.push_back(MpegStream);

        // Add the stream to the outputs and create the VBR frame
        m_Outputs.push_back(elMpegStream());
        m_Outputs.back().push_back(elMpegFrame());

        elMpegFrame& VbrFrame = m_Outputs.back().back();
        memset(VbrFrame.Data.get(), 0x11, MAX_MPEG_FRAME_BUFFER);
        ConstructMpegVbrFrame(Streams[i][0].Gr, VbrFrame, 0, 0);
    }

    // Make sure that there is at least one MPEG stream
    if (!m_StreamInfo.size())
    {
        return false;
    }

    // Initialize some vars
    m_Streams.clear();
    m_CurrentFrame = 0;
    m_UncompressedSampleFrames = 0;
    m_DoneParsingBlocks = false;
    m_CurMpegFrame = 1;
    m_CurOutputMpegFrame = 0;
    m_SampleFrames = 0;
    return true;
}

unsigned int elMpegGenerator::GetStreamCount() const
{
    return m_StreamInfo.size();
}

unsigned long elMpegGenerator::GetUncSampleFrameCount() const
{
    return m_UncompressedSampleFrames;
}

long unsigned elMpegGenerator::GetSampleFrameCount() const
{
    return m_SampleFrames;
}

unsigned int elMpegGenerator::GetSampleRate(unsigned int StreamIndex) const
{
    if (StreamIndex >= m_StreamInfo.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    return m_StreamInfo[StreamIndex].SampleRate;
}

unsigned int elMpegGenerator::GetChannels(unsigned int StreamIndex) const
{
    if (StreamIndex >= m_StreamInfo.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    return m_StreamInfo[StreamIndex].Channels;
}


void elMpegGenerator::ParseBlock(const elBlock& Block)
{
    // Sanity check
    if (m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Already called DoneParsingBlocks(), can't parse any more blocks."));
    }

    m_SampleFrames += Block.SampleCount;
    VERY_VERBOSE("Block offset: " << Block.Offset << "; Block size: " << Block.Size << "; Sample count: " << Block.SampleCount);

    // Read the block data
    bsBitstream IS(Block.Data.get(), Block.Size);
    ReadBlockData(m_Streams, IS);

    // Create a frame for each stream
    unsigned int OldCurMpegFrame = m_CurMpegFrame;
    for (unsigned int i = 0; i < m_StreamInfo.size(); i++)
    {
        const elStream& CurStr = m_Streams[i];

        // The current frame index
        m_CurMpegFrame = OldCurMpegFrame;

        while (CurStr.size())
        {
            // Get the current and previous frames
            m_Outputs[i].push_back(elMpegFrame());
            elMpegFrame& CurOutFrame = m_Outputs[i][m_CurMpegFrame];

            ConstructMpegFrame(CurStr[0], IS, CurOutFrame);
            if (CurOutFrame.Used == 0)
            {
                m_Outputs[i].pop_back();
                break;
            }
            else
            {
                m_CurMpegFrame++;
                m_Streams[i].pop_front();
            }
        }
    }
    return;
}

void elMpegGenerator::DoneParsingBlocks()
{
    // Make sure we didn't call this before
    if (m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Already called DoneParsingBlocks()"));
    }

    // Write the VBR frame again for each stream
    for (unsigned int i = 0; i < m_StreamInfo.size(); i++)
    {
        // Get the total file size
        unsigned long FileSize = 0;

        // Go backwards through the frames and calculate their bitrates
        for (unsigned int j = m_Outputs[i].size(); j > 0; j--)
        {
            elMpegFrame& Frame = m_Outputs[i][j - 1];

            unsigned int FrameUsed;
            unsigned int BitrateIndex;
            FrameUsed = Frame.Used + Frame.UsedByNext;
            BitrateIndex = EstimateBitrateIndex(FrameUsed, Frame.SampleRate, Frame.Version);

            if (BitrateIndex > 0)
            {
                Frame.Size = CalculateFrameSize(BitrateIndex, Frame.SampleRate, Frame.Version);
                Frame.UsedFromPrevious = 0;
            }
            else if (j > 1)
            {
                elMpegFrame& PrevFrame = m_Outputs[i][j - 2];

                // Use the highest bitrate
                BitrateIndex = 14;
                Frame.Size = CalculateFrameSize(BitrateIndex, Frame.SampleRate, Frame.Version);

                // Use some bytes from the previous frame
                const unsigned int BytesNeed = FrameUsed - Frame.Size;
                Frame.UsedFromPrevious = BytesNeed;
                PrevFrame.UsedByNext = BytesNeed;
            }
            else
            {
                throw (elMpegGeneratorException("Was unable to construct MPEG audio frame. The bitrate exceeded the maximum."));
            }

            // Write
            WriteFields(Frame, BitrateIndex, Frame.UsedFromPrevious);
            FileSize += Frame.Size;
        }

        ConstructMpegVbrFrame(NULL, m_Outputs[i][0], m_Outputs[i].size(), FileSize);
    }

    m_CurMpegFrame = 0;
    m_CurOutputMpegFrame = 0;
    m_DoneParsingBlocks = true;
    return;
}

shared_ptr<elMpegOutputStream> elMpegGenerator::CreateMpegStream(unsigned int StreamIndex) const
{
    // Check some things
    if (!m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Haven't called DoneParsingBlocks(), we're not done parsing blocks."));
    }
    if (StreamIndex >= m_Outputs.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    return shared_ptr<elMpegOutputStream>(new elMpegOutputStream(*this, StreamIndex));
}

shared_ptr<elPcmOutputStream> elMpegGenerator::CreatePcmStream(unsigned int StreamIndex) const
{
    // Check some things
    if (!m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Haven't called DoneParsingBlocks(), we're not done parsing blocks."));
    }
    if (StreamIndex >= m_Outputs.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    return shared_ptr<elPcmOutputStream>(new elPcmOutputStream(*this, StreamIndex));
}

unsigned int elMpegGenerator::GetFrameCount(unsigned int StreamIndex) const
{
    // Check some things
    if (!m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Haven't called DoneParsingBlocks(), we're not done parsing blocks."));
    }
    if (StreamIndex >= m_Outputs.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    return m_Outputs[StreamIndex].size();
}


unsigned int elMpegGenerator::ReadFrame(uint8_t* Buffer, unsigned int BufferSize, unsigned int Index, unsigned int StreamIndex) const
{
    // Check some things
    if (!m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Haven't called DoneParsingBlocks(), we're not done parsing blocks."));
    }
    if (StreamIndex >= m_Outputs.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    if (!m_Outputs[StreamIndex].size())
    {
        throw (elMpegGeneratorException("No frames are outputted."));
    }
    if (Index >= m_Outputs[StreamIndex].size())
    {
        throw (elMpegGeneratorException("Current frame is past the end of the stream."));
    }

    // The frame
    const elMpegFrame& Frame = m_Outputs[StreamIndex][Index];
    unsigned int ToCopy;
    const uint8_t* OldBuffer = Buffer;

    // Write the header
    ToCopy = min(Frame.HeaderSize, BufferSize);

    memcpy(Buffer, Frame.Data.get(), ToCopy);

    BufferSize -= ToCopy;
    Buffer += ToCopy;

    // Write part of the main data
    ToCopy = min(Frame.Used - Frame.HeaderSize - Frame.UsedFromPrevious, BufferSize);

    memcpy(Buffer, Frame.Data.get() + Frame.HeaderSize + Frame.UsedFromPrevious, ToCopy);

    BufferSize -= ToCopy;
    Buffer += ToCopy;

    // Write the padding
    ToCopy = min(Frame.Size - (Frame.Used - Frame.UsedFromPrevious) - Frame.UsedByNext,
                 BufferSize);

    memset(Buffer, 0xE5, ToCopy);

    BufferSize -= ToCopy;
    Buffer += ToCopy;

    // Finally write any bytes used by the next frame
    if (Frame.UsedByNext > 0)
    {
        const elMpegFrame& NextFrame = m_Outputs[StreamIndex][Index + 1];
        assert(Frame.UsedByNext == NextFrame.UsedFromPrevious);

        ToCopy = min(Frame.UsedByNext, BufferSize);

        memcpy(Buffer, NextFrame.Data.get() + NextFrame.HeaderSize, ToCopy);

        BufferSize -= ToCopy;
        Buffer += ToCopy;
    }

    assert(Buffer - OldBuffer == Frame.Size);
    return Frame.Size;
}

const elUncompressedSampleFrames& elMpegGenerator::ReadUncSamples(unsigned int Granule, unsigned int Index, unsigned int StreamIndex) const
{
    // Check some things
    if (!m_DoneParsingBlocks)
    {
        throw (elMpegGeneratorException("Haven't called DoneParsingBlocks(), we're not done parsing blocks."));
    }
    if (StreamIndex >= m_Outputs.size())
    {
        throw (elMpegGeneratorException("Stream index exceeds the number of streams."));
    }
    if (!m_Outputs[StreamIndex].size())
    {
        throw (elMpegGeneratorException("No frames are outputted."));
    }
    if (Index >= m_Outputs[StreamIndex].size())
    {
        throw (elMpegGeneratorException("Current frame is past the end of the stream."));
    }

    if (Granule == 1)
    {
        return m_Outputs[StreamIndex][Index].UncompB;
    }
    return m_Outputs[StreamIndex][Index].UncompA;
}


void elMpegGenerator::ReadBlockData(elStreamVector& Streams, bsBitstream& IS)
{
    m_Parser->Parse(Streams, IS);
    
#ifdef ENABLE_VERY_VERBOSE
    if (g_Verbose >= 2)
    {
        Print(Streams);
    }
#endif

    m_CurrentFrame += Streams[0].size();
    return;
}

void elMpegGenerator::ConstructMpegVbrFrame(const elGranule* Granule, elMpegFrame& Out, unsigned int Frames, unsigned int DataSize)
{
    bsBitstream OS(Out.Data.get(), MAX_MPEG_FRAME_BUFFER);

    // Get some stuff
    if (Granule)
    {
        Out.Version = Granule->Version;
        Out.Channels = Granule->Channels;
        Out.SampleRate = Granule->SampleRate;
    }

    // Calcluate how much space will be used
    const unsigned int SideInfoSize = CalculateSideInfoSize(Out.Channels, Out.Version);
    Out.Used = 4;
    Out.Used += SideInfoSize;
    Out.Used += 4 * 4;
    Out.HeaderSize = Out.Used;

    // Write the MPEG frame header if we have the information
    if (Granule)
    {
        OS.WriteBits(0x7FF, 11);                // Frame sync
        OS.WriteBits(Granule->Version, 2);      // Version
        OS.WriteBits(0x1, 2);                   // Layer
        OS.WriteBit(1);                         // CRC protection
        OS.WriteBits(0, 4);                     // Bitrate index
        OS.WriteBits(Granule->SampleRateIndex, 2); // Sample rate index
        OS.WriteBit(0);                         // Padding
        OS.WriteBit(0);                         // Private bit
        OS.WriteBits(Granule->ChannelMode, 2);  // Channel mode
        OS.WriteBits(Granule->ModeExtension, 2);// Channel mode extension
        OS.WriteBit(1);                         // Copyrighted
        OS.WriteBit(1);                         // Original
        OS.WriteBits(0, 2);                     // Emphasis
    }

    // Write the side info (zeros)
    OS.SeekAbsolute(32);
    for (unsigned int i = 0; i < SideInfoSize; i++)
    {
        OS.WriteAligned8<uint8_t>(0);
    }

    // Write the info
    OS.WriteAligned8<char>('X');
    OS.WriteAligned8<char>('i');
    OS.WriteAligned8<char>('n');
    OS.WriteAligned8<char>('g');
    OS.WriteAligned32BE<uint32_t>(VBR_FRAMES_FLAG | VBR_BYTES_FLAG);
    OS.WriteAligned32BE<uint32_t>(Frames);
    OS.WriteAligned32BE<uint32_t>(DataSize);
    return;
}


void elMpegGenerator::ConstructMpegFrame(const elFrame& Fr, bsBitstream& IS, elMpegGenerator::elMpegFrame& Out)
{
    switch (Fr.Gr[0].Version)
    {
        case MV_1:
            ConstructMpegFrameV1(Fr, IS, Out);
        break;
        case MV_2:
        case MV_2_5:
            ConstructMpegFrameV2(Fr, IS, Out);
        break;
        default:
            throw (elMpegGeneratorException("Invalid version passed to ConstructMpegFrame."));
    }
    return;
}

void elMpegGenerator::ConstructMpegFrameV1(const elFrame& Fr, bsBitstream& IS, elMpegFrame& Out)
{
    const elGranule& BaseGr = Fr.Gr[0];

    // Check the version to be sure
    if (BaseGr.Version != MV_1)
    {
        throw (elMpegGeneratorException("Wrong version called on ConstructMpegFrameV1."));
    }

    // If we don't have a full frame, jump ship
    if (!Fr.Gr[0].Used || !Fr.Gr[1].Used)
    {
        VERBOSE("G: we only have one granule, not enough for a frame");
        Out.Used = 0;
        Out.Size = 0;
        return;
    }

    // Calculate the amount of data this frame will use
    Out.Used = 4;
    Out.Used += CalculateSideInfoSize(BaseGr.Channels, BaseGr.Version);
    Out.HeaderSize = Out.Used;

    unsigned long DataBitCount = 0;
    for (unsigned int i = 0; i < 2; i++)
    {
        for (unsigned int j = 0; j < BaseGr.Channels; j++)
        {
            DataBitCount += Fr.Gr[i].ChannelInfo[j].Size;
        }
    }
    if (DataBitCount % 8)
    {
        DataBitCount += 8 - DataBitCount % 8;
    }

    Out.Used += DataBitCount / 8;

    Out.UncompA = Fr.Gr[0].Uncomp;
    Out.UncompB = Fr.Gr[1].Uncomp;

    // Write the MPEG header
    bsBitstream OS(Out.Data.get(), MAX_MPEG_FRAME_BUFFER);
    unsigned int Padding = 0;

    OS.WriteBits(0x7FF, 11);                    // Frame sync
    OS.WriteBits(BaseGr.Version, 2);            // Version
    OS.WriteBits(0x1, 2);                       // Layer
    OS.WriteBit(1);                             // CRC protection
    OS.WriteBits(0, 4);                         // Bitrate index
    OS.WriteBits(BaseGr.SampleRateIndex, 2);    // Sample rate index
    OS.WriteBit(Padding);                       // Padding
    OS.WriteBit(0);                             // Private bit
    OS.WriteBits(BaseGr.ChannelMode, 2);        // Channel mode
    OS.WriteBits(BaseGr.ModeExtension, 2);      // Channel mode extension
    OS.WriteBit(1);                             // Copyrighted
    OS.WriteBit(1);                             // Original
    OS.WriteBits(0, 2);                         // Emphasis

    m_UncompressedSampleFrames += Fr.Gr[0].Uncomp.Count;
    m_UncompressedSampleFrames += Fr.Gr[1].Uncomp.Count;

    Out.Version = BaseGr.Version;
    Out.SampleRate = BaseGr.SampleRate;
    Out.Channels = BaseGr.Channels;

    // Write the beginning of the side info
    OS.WriteBits(0, CalculateMainDataStartBits(Out.Version));
    OS.WriteBits(0, CalculatePrivateBits(Out.Channels, Out.Version));

    // Write the scfsi
    for (unsigned int i = 0; i < Fr.Gr[1].Channels; i++)
    {
        OS.WriteBits(Fr.Gr[1].ChannelInfo[i].Scfsi, 4);
    }

    // Write the rest of the side info
    for (unsigned int i = 0; i < 2; i++)
    {
        for (unsigned int j = 0; j < Fr.Gr[i].Channels; j++)
        {
            OS.WriteBits(Fr.Gr[i].ChannelInfo[j].Size, 12);
            OS.WriteBits(Fr.Gr[i].ChannelInfo[j].SideInfo[0], 32);
            OS.WriteBits(Fr.Gr[i].ChannelInfo[j].SideInfo[1], 47 - 32);
        }
    }

    // Now write the actual data
    for (unsigned int i = 0; i < 2; i++)
    {
        if (Fr.Gr[i].DataSize == 0)
        {
            continue;
        }
        
        bsBitstream DataReader(Fr.Gr[i].Data.get(), Fr.Gr[i].DataSize);

        for (unsigned int j = 0; j < BaseGr.Channels; j++)
        {
            unsigned int BitsLeft = Fr.Gr[i].ChannelInfo[j].Size;

            while (BitsLeft)
            {
                unsigned int BitsToRead = min(32, BitsLeft);
                uint32_t Bits = DataReader.ReadBits(BitsToRead);
                OS.WriteBits(Bits, BitsToRead);
                BitsLeft -= BitsToRead;
            }
        }
    }

    // Pad to the nearest byte
    OS.WriteToNextByte();
    return;
}

void elMpegGenerator::ConstructMpegFrameV2(const elFrame& Fr, bsBitstream& IS, elMpegFrame& Out)
{
    const elGranule& BaseGr = Fr.Gr[0];

    // Check the version to be sure
    if (BaseGr.Version != MV_2 && BaseGr.Version != MV_2_5)
    {
        throw (elMpegGeneratorException("Wrong version called on ConstructMpegFrameV2."));
    }

    // If we don't have a full frame, jump ship
    if (!BaseGr.Used)
    {
        VERBOSE("G: we only have one granule, not enough for a frame");
        Out.Used = 0;
        Out.Size = 0;
        return;
    }

    // Calculate the amount of data this frame will use
    Out.Used = 4;
    Out.Used += CalculateSideInfoSize(BaseGr.Channels, BaseGr.Version);
    Out.HeaderSize = Out.Used;

    unsigned long DataBitCount = 0;
    for (unsigned int j = 0; j < BaseGr.Channels; j++)
    {
        DataBitCount += BaseGr.ChannelInfo[j].Size;
    }

    if (DataBitCount % 8)
    {
        DataBitCount += 8 - DataBitCount % 8;
    }

    Out.Used += DataBitCount / 8;

    // Write the MPEG header
    bsBitstream OS(Out.Data.get(), MAX_MPEG_FRAME_BUFFER);
    unsigned int Padding = 0;

    OS.WriteBits(0x7FF, 11);                    // Frame sync
    OS.WriteBits(BaseGr.Version, 2);            // Version
    OS.WriteBits(0x1, 2);                       // Layer
    OS.WriteBit(1);                             // CRC protection
    OS.WriteBits(0, 4);                         // Bitrate index
    OS.WriteBits(BaseGr.SampleRateIndex, 2);    // Sample rate index
    OS.WriteBit(Padding);                       // Padding
    OS.WriteBit(0);                             // Private bit
    OS.WriteBits(BaseGr.ChannelMode, 2);        // Channel mode
    OS.WriteBits(BaseGr.ModeExtension, 2);      // Channel mode extension
    OS.WriteBit(1);                             // Copyrighted
    OS.WriteBit(1);                             // Original
    OS.WriteBits(0, 2);                         // Emphasis

    m_UncompressedSampleFrames += BaseGr.Uncomp.Count;

    Out.Version = BaseGr.Version;
    Out.SampleRate = BaseGr.SampleRate;
    Out.Channels = BaseGr.Channels;

    // Write the beginning of the side info
    OS.WriteBits(0, CalculateMainDataStartBits(Out.Version));
    OS.WriteBits(0, CalculatePrivateBits(Out.Channels, Out.Version));

    // Write the rest of the side info
    for (unsigned int j = 0; j < BaseGr.Channels; j++)
    {
        OS.WriteBits(BaseGr.ChannelInfo[j].Size, 12);
        OS.WriteBits(BaseGr.ChannelInfo[j].SideInfo[0], 32);
        OS.WriteBits(BaseGr.ChannelInfo[j].SideInfo[1], 51 - 32);
    }

    // Now write the actual data
    if (BaseGr.DataSize > 0)
    {
        bsBitstream DataReader(BaseGr.Data.get(), BaseGr.DataSize);

        for (unsigned int j = 0; j < BaseGr.Channels; j++)
        {
            unsigned int BitsLeft = BaseGr.ChannelInfo[j].Size;

            while (BitsLeft)
            {
                unsigned int BitsToRead = min(32, BitsLeft);
                uint32_t Bits = DataReader.ReadBits(BitsToRead);
                OS.WriteBits(Bits, BitsToRead);
                BitsLeft -= BitsToRead;
            }
        }
    }

    // Pad to the nearest byte
    OS.WriteToNextByte();
    return;
}

unsigned int elMpegGenerator::EstimateBitrateIndex(unsigned int FrameUsed, unsigned int SampleRate, unsigned int Version)
{
    for (unsigned int i = 0; i < 15; i++)
    {
        if (CalculateFrameSize(i, SampleRate, Version) >= FrameUsed)
        {
            return i;
        }
    }
    return 0;
}

unsigned int elMpegGenerator::CalculateFrameSize(unsigned int BitrateIndex, unsigned int SampleRate, unsigned int Version)
{
    switch (Version)
    {
        case MV_1:
            return 144000 * MpegBitrateTable[Version][BitrateIndex] / SampleRate;
        case MV_2:
        case MV_2_5:
            return 144000 * MpegBitrateTable[Version][BitrateIndex] / (SampleRate * 2);
        default:
            throw (elMpegGeneratorException("Invalid version passed to CalculateFrameSize."));
    }
    return 0;
}

unsigned int elMpegGenerator::CalculateSideInfoSize(unsigned int Channels, unsigned int Version)
{
    switch (Version)
    {
        case MV_1:
            return Channels == 1 ? 17 : 32;
        case MV_2:
        case MV_2_5:
            return Channels == 1 ? 9 : 17;
        default:
            throw (elMpegGeneratorException("Invalid version passed to CalculateSideInfoSize."));
    };
    return 0;
}

unsigned int elMpegGenerator::CalculatePrivateBits(unsigned int Channels, unsigned int Version)
{
    switch (Version)
    {
        case MV_1:
            return Channels == 1 ? 5 : 3;
        case MV_2:
        case MV_2_5:
            return Channels == 1 ? 1 : 2;
        default:
            throw (elMpegGeneratorException("Invalid version passed to CalculatePrivateBits."));
    };
    return 0;
}

unsigned int elMpegGenerator::CalculateMainDataStartBits(unsigned int Version)
{
    switch (Version)
    {
        case MV_1:
            return 9;
        case MV_2:
        case MV_2_5:
            return 8;
        default:
            throw (elMpegGeneratorException("Invalid version passed to CalculateMainDataStartBits."));
    };
    return 0;
}

void elMpegGenerator::WriteFields(elMpegFrame& Frame, unsigned int NewBitrateIndex, unsigned int NewUsedFromPrev) const
{
    bsBitstream OS(Frame.Data.get(), 8);
    OS.SeekAbsolute(16);
    OS.WriteBits(NewBitrateIndex, 4);
    OS.SeekAbsolute(32);
    OS.WriteBits(NewUsedFromPrev, CalculateMainDataStartBits(Frame.Version));
    return;
}

// Helper functions

void elMpegGenerator::Print(const elGranule& Gr, const std::string& Indent)
{
    std::cout << Indent << "UncompressedSamples: " << (Gr.Uncomp.Count ? "yes" : "no") << std::endl;
    std::cout << Indent << "Version: " << (int)Gr.Version << std::endl;
    std::cout << Indent << "SampleRateIndex: " << (int)Gr.SampleRateIndex << std::endl;
    std::cout << Indent << "SampleRate: " << Gr.SampleRate << std::endl;
    std::cout << Indent << "ChannelMode: " << (int)Gr.ChannelMode << std::endl;
    std::cout << Indent << "Channels: " << (int)Gr.Channels << std::endl;
    std::cout << Indent << "ModeExtension: " << (int)Gr.ModeExtension << std::endl;
    std::cout << Indent << "Granule: " << (int)Gr.Index << std::endl;
    std::cout << Indent << "UncompressedSampleCount: " << Gr.Uncomp.Count << std::endl;
    std::cout << Indent << "UncompressedSampleOffset: " << Gr.Uncomp.OffsetInOutput << std::endl;
    //std::cout << Indent << "Data offset: " << Gr.DataOffset << std::endl;

    if (Gr.Index == 1)
    {
        std::cout << Indent << "Scfsi: ";
        for (unsigned int i = 0; i < Gr.Channels; i++)
        {
            if (i)
            {
                std::cout << ", ";
            }
            std::cout << Gr.ChannelInfo[i].Scfsi;
        }
        std::cout << std::endl;
    }

    std::cout << Indent << "Size: ";
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        if (i)
        {
            std::cout << ", ";
        }
        std::cout << Gr.ChannelInfo[i].Size;
    }
    std::cout << std::endl;
    return;
}

void elMpegGenerator::Print(const elStreamVector& Streams)
{
    unsigned int I1 = 0;
    for (elStreamVector::const_iterator Iter1 = Streams.begin();
            Iter1 != Streams.end(); ++Iter1)
    {
        std::cout << "Stream #" << I1 << ": " << std::endl;

        unsigned int I2 = 0;
        for (elStream::const_iterator Iter2 = Iter1->begin();
                Iter2 != Iter1->end(); ++Iter2)
        {
            std::cout << "    Frame #" << I2 << " (" << I2 + m_CurrentFrame << "): " << std::endl;

            for (unsigned int i = 0; i < 2; i++)
            {
                const elGranule& Gr = Iter2->Gr[i];

                std::cout << "        Granule #" << i << ": " << std::endl;
                Print(Gr, "            ");
            }

            std::cout << std::endl;
            I2++;
        }

        std::cout << std::endl;
        I1++;
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << std::endl << std::endl;
    return;
}

elMpegGeneratorException::elMpegGeneratorException(const std::string& What) throw() :
        m_What(What)
{
    return;
}

elMpegGeneratorException::~elMpegGeneratorException() throw()
{
    return;
}

const char* elMpegGeneratorException::what() const throw()
{
    return m_What.c_str();
}
