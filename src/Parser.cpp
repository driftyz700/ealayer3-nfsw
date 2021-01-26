/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "Parser.h"
#include "Bitstream.h"

static const unsigned int MpegSampleRateTable[4][4] = {
    {11025, 12000, 8000, 0},
    {0, 0, 0, 0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0}
};

elParser::elParser() :
    m_CurrentFrame(0)
{
    return;
}

elParser::~elParser()
{
    return;
}

const std::string elParser::GetName() const
{
    return "EAL3 ver. 5";
}

bool elParser::Initialize(bsBitstream& IS)
{
    bool First = true;
    try
    {
        while (!IS.Eos())
        {
            elGranule Gr;
            if (!ReadGranuleWithUncSamples(IS, Gr))
            {
                if (First)
                {
                    throw (elParserException("There aren't any granules."));
                }
                break;
            }
            First = false;
        }
    }
    catch (elParserException& E)
    {
        VERBOSE("P: " << GetName() << " incorrect with exception: " << E.what());
        return false;
    }
    VERBOSE("P: " << GetName() << " correct");
    return true;
}

inline void PutStreamOnBack(elStreamVector& Streams, unsigned int CurrentStream)
{
    if (CurrentStream == Streams.size())
    {
        Streams.push_back(elStream());
    }
    else if (CurrentStream > Streams.size())
    {
        throw (elParserException("Bug in this program! (PutStreamOnBack)"));
    }
    return;
}

inline void PutFrameOnBack(elStream& Frames, unsigned int CurrentFrame)
{
    if (CurrentFrame == Frames.size())
    {
        Frames.push_back(elFrame());
    }
    else if (CurrentFrame > Frames.size())
    {
        throw (elParserException("Bug in this program! (PutFrameOnBack)"));
    }
    return;
}

void elParser::Parse(elStreamVector& Streams, bsBitstream& IS)
{
    unsigned int CurrentStream = 0;
    unsigned int CurrentGranule = 0;
    unsigned int CurrentFrame = 0;
    while (!IS.Eos())
    {
        // Read a granule
        elGranule Gr;
        if (!ReadGranuleWithUncSamples(IS, Gr))
        {
            break;
        }

        // Figure out where to put it
        if (Gr.Index != CurrentGranule)
        {
            CurrentGranule = Gr.Index;
            CurrentStream = 0;

            PutStreamOnBack(Streams, CurrentStream);

            if (Gr.Index == 0)
            {
                CurrentFrame++;
                for (elStreamVector::iterator Str = Streams.begin(); Str != Streams.end(); ++Str)
                {
                    Str->push_back(elFrame());
                }
            }

            // Set the granule only if it's used
            if (Gr.Used)
            {
                Streams[CurrentStream][CurrentFrame].Gr[CurrentGranule] = Gr;
            }
        }
        else
        {
            PutStreamOnBack(Streams, CurrentStream);
            PutFrameOnBack(Streams[CurrentStream], CurrentFrame);

            // Set the granule only if it's used
            if (Gr.Used)
            {
                Streams[CurrentStream][CurrentFrame].Gr[CurrentGranule] = Gr;
            }
        }

        if (Gr.Version == MV_1)
        {
            CurrentStream++;
        }
        else
        {
            CurrentFrame++;
        }
    }
    return;
}

bool elParser::ReadGranuleWithUncSamples(bsBitstream& IS, elGranule& Gr)
{
    if (IS.Eos())
    {
        return false;
    }

    // See if there are any uncompressed samples at the end
    unsigned int UncompressedSamples = IS.ReadBits(8);
    
    if (!ReadGranule(IS, Gr))
    {
        return false;
    }

    // Check if this is the last granule in the block
    if (Gr.Version == 0 && Gr.SampleRateIndex == 0 && Gr.ChannelMode == 0 &&
        Gr.ModeExtension == 0 && Gr.Index == 0)
    {
        VERBOSE("P: " << GetName() << ": null granule encountered, end of stream");
        return false;
    }

    // Read in the uncompressed samples
    IS.SeekToNextByte();
    if (UncompressedSamples)
    {
        Gr.Uncomp.Count = IS.ReadAligned32BE<unsigned int>();
        Gr.Uncomp.OffsetInOutput = IS.ReadAligned32BE<unsigned int>();
        ReadUncSamples(IS, Gr);
    }
    else
    {
        Gr.Uncomp.Count = 0;
        Gr.Uncomp.OffsetInOutput = 0;
    }
        
    Gr.Used = true;
    return true;
}

bool elParser::ReadGranule(bsBitstream& IS, elGranule& Gr)
{
    if (IS.Eos())
    {
        return false;
    }

    // Read some fields in
    Gr.Version = IS.ReadBits(2);
    Gr.SampleRateIndex = IS.ReadBits(2);
    Gr.ChannelMode = IS.ReadBits(2);
    Gr.ModeExtension = IS.ReadBits(2);
    Gr.Index = IS.ReadBit();

    // Are we at the end of the block?
    if (Gr.Version == 0 && Gr.SampleRateIndex == 0 && Gr.ChannelMode == 0 &&
        Gr.ModeExtension == 0 && Gr.Index == 0)
    {
        VERBOSE("P: " << GetName() << ": null granule encountered, end of block");
        Gr.Used = false;
        return false;
    }

    // Check for integrity and set other members
    if (Gr.Version == MV_RESERVED)
    {
        throw (elParserException("Version field invalid."));
    }
    if (Gr.SampleRateIndex == 3)
    {
        throw (elParserException("Sample rate index field invalid."));
    }
    if (Gr.Version != MV_1)
    {
        Gr.Index = 0;
    }
    Gr.SampleRate = MpegSampleRateTable[Gr.Version][Gr.SampleRateIndex];
    Gr.Channels = Gr.ChannelMode == CM_MONO ? 1 : 2;

    // Prepare the channel info array
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        elChannelInfo Channel;
        Channel.Scfsi = 0;
        Channel.Size = 0;
        Gr.ChannelInfo.push_back(Channel);
    }

    // Read in scfsi
    if (Gr.Index == 1 && Gr.Version == MV_1)
    {
        for (unsigned int i = 0; i < Gr.Channels; i++)
        {
            Gr.ChannelInfo[i].Scfsi = IS.ReadBits(4);
        }
    }

    // Read in the side info
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        Gr.ChannelInfo[i].Size = IS.ReadBits(12);
        Gr.ChannelInfo[i].SideInfo[0] = IS.ReadBits(32);
        if (Gr.Version == MV_1)
        {
            Gr.ChannelInfo[i].SideInfo[1] = IS.ReadBits(47 - 32);
        }
        else
        {
            Gr.ChannelInfo[i].SideInfo[1] = IS.ReadBits(51 - 32);
        }
    }

    // Get the data size
    unsigned int DataBitCount = 0;
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        DataBitCount += Gr.ChannelInfo[i].Size;
    }
    
    if (DataBitCount > IS.GetCountBitsLeft())
    {
        throw (elParserException("Data goes beyond end of stream."));
    }

    Gr.DataSize = DataBitCount;
    if (Gr.DataSize % 8)
    {
        Gr.DataSize += 8 - DataBitCount % 8;
    }
    Gr.DataSize /= 8;

    // Read in the data
    if (Gr.DataSize)
    {
        Gr.Data = shared_array<uint8_t>(new uint8_t[Gr.DataSize]);

        bsBitstream OS(Gr.Data.get(), Gr.DataSize);
        while (DataBitCount)
        {
            unsigned int BitsToRead = min(32, DataBitCount);
            uint32_t Bits = IS.ReadBits(BitsToRead);
            OS.WriteBits(Bits, BitsToRead);
            DataBitCount -= BitsToRead;
        }
        OS.WriteToNextByte();
    }
    else
    {
        Gr.Data.reset();
    }
    
    Gr.Used = true;
    return true;
}

void elParser::ReadUncSamples(bsBitstream& IS, elGranule& Gr)
{
    if (Gr.Uncomp.Count == 0)
    {
        return;
    }

    // First make sure that this is a valid number of samples
    const unsigned int NumberOfSamples = Gr.Uncomp.Count * Gr.Channels;

    IS.SeekToNextByte();
    if (NumberOfSamples * 2 * 8 > IS.GetCountBitsLeft())
    {
        throw (elParserException("The number of uncompressed samples exceeds the amount of data left."));
    }

    // Allocate data for them
    Gr.Uncomp.Data = shared_array<short>(new short[NumberOfSamples]);

    // Read in the samples, interleaving them
    for (unsigned int i = 0; i < Gr.Channels; i++)
    {
        for (unsigned int j = 0; j < Gr.Uncomp.Count; j++)
        {
            Gr.Uncomp.Data[j * Gr.Channels + i] = IS.ReadAligned16BE<short>();
        }
    }
    return;
}

elParserException::elParserException(const std::string& What) throw() :
        m_What(What)
{
    return;
}

elParserException::~elParserException() throw()
{
    return;
}

const char* elParserException::what() const throw()
{
    return m_What.c_str();
}
