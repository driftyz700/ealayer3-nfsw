/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010-2011, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "MpegParser.h"
#include "Parser.h"
#include "Bitstream.h"
#include "MpegGenerator.h"

static const unsigned int MpegSampleRateTable[4][4] = {
    {11025, 12000, 8000, 0},
    {0, 0, 0, 0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0}
};

elMpegParser::elMpegParser() :
    m_ReservoirUsed(0)
{
    return;
}

elMpegParser::~elMpegParser()
{
    return;
}

void elMpegParser::Initialize(std::istream* Input)
{
    assert(Input);
    m_Input = Input;
    m_ReservoirUsed = 0;

    // TODO: Make sure this really is an MP3 file
    return;
}

bool elMpegParser::ReadFrame(elFrame& Frame)
{
    assert(m_Input);

    Frame.Gr[0].Used = false;
    Frame.Gr[1].Used = false;
    Frame.Gr[0].Uncomp.Data.reset();
    Frame.Gr[0].Uncomp.Count = 0;
    Frame.Gr[1].Uncomp.Data.reset();
    Frame.Gr[1].Uncomp.Count = 0;

    // Are there any frames left?
    if (!FramesLeft())
    {
        return false;
    }

    // Figure out what kind of frame this is
    uint8_t FrameHeader[10];
    std::streamoff StartOffset;
    m_Input->clear();
    StartOffset = m_Input->tellg();
    m_Input->read((char*)FrameHeader, 10);
    m_Input->seekg(StartOffset);

    // Based on what this is, process it.
    if (memcmp(FrameHeader, "ID3", 3) == 0)
    {
        SkipID3Tag(FrameHeader);
    }
    else if (FrameHeader[0] == 0xFF)
    {
        //m_Input->seekg(StartOffset + 4);
        elRawFrameHeader RawFrameHeader;
        if (!ProcessFrameHeader(RawFrameHeader, FrameHeader))
        {
            return false;
        }
        if (!ProcessMpegFrame(Frame, RawFrameHeader))
        {
            return false;
        }
    }
    else
    {
        // Try to find the capture pattern in the first 2000 bytes.
        VERBOSE("Trying to find the next frame... (ignore message if at the end of file)");
        for (unsigned int i = 0; i < 2000 && !m_Input->eof(); i++)
        {
            StartOffset = m_Input->tellg();
            FrameHeader[0] = m_Input->get();
            
            if (FrameHeader[0] == 0xFF)
            {
                VERBOSE("Found a frame!");
                m_Input->read((char*)(FrameHeader + 1), 10 - 1);
                m_Input->seekg(StartOffset);

                try
                {
                    elRawFrameHeader RawFrameHeader;
                    if (!ProcessFrameHeader(RawFrameHeader, FrameHeader))
                    {
                        return false;
                    }
                    if (!ProcessMpegFrame(Frame, RawFrameHeader))
                    {
                        return false;
                    }
                }
                catch (std::exception& E)
                {
                    VERBOSE("Exception finding frame (doesn't matter if at the end of the file): " << E.what());
                    return false;
                }
                return true;
            }
        }
        VERBOSE("Not found.");
        return false;
    }
    return true;
}

bool elMpegParser::FramesLeft() const
{
    assert(m_Input);
    return !m_Input->eof();
}

void elMpegParser::SkipID3Tag(uint8_t FrameHeader[10])
{
    // Read the number from the frame header
    uint8_t TempNumber[4];
    bsBitstream IS(FrameHeader, 10);
    bsBitstream Temp(TempNumber, 4);
    IS.SeekAbsolute(6 * 8);
    Temp.WriteBits(0, 4);

    for (unsigned int i = 0; i < 4; i++)
    {
        IS.ReadBit();
        Temp.WriteBits(IS.ReadBits(7), 7);
    }
    Temp.WriteToNextByte();

    // Get the size
    unsigned int Size;
    Temp.SeekAbsolute(0);
    Size = Temp.ReadAligned32BE<unsigned int>();

    VERBOSE("ID3 Tag size: " << Size);

    // Finally seek past it.
    m_Input->seekg(Size + 10, std::ios_base::cur);
    return;
}

bool elMpegParser::ProcessFrameHeader(elRawFrameHeader& Fr, uint8_t FrameHeader[10])
{
    bsBitstream IS(FrameHeader, 10);
    
    // Read the fields.
    if (IS.ReadBits(11) != 0x7FF)
    {
        throw (elMpegParserException("MPEG sync bits don't match. Keep in mind that for this program to work the MP3 must be well-formed."));
    }
    
    Fr.Version = IS.ReadBits(2);
    
    if (IS.ReadBits(2) != 0x1)
    {
        throw (elMpegParserException("File not supported; only MPEG layer 3 is supported."));
    }
    
    Fr.Crc = (IS.ReadBit() == 0) ? true : false;
    Fr.BitrateIndex = IS.ReadBits(4);
    Fr.SampleRateIndex = IS.ReadBits(2);
    Fr.Padding = (IS.ReadBit() == 1) ? true : false;
    
    IS.ReadBit();
    
    Fr.ChannelMode = IS.ReadBits(2);
    Fr.ModeExtension = IS.ReadBits(2);
    
    IS.ReadBits(4);

    // Calculate the header size.
    Fr.HeaderSize = 4 + (Fr.Crc ? 2 : 0);

    // Calculate the sample rate.
    Fr.SampleRate = MpegSampleRateTable[Fr.Version][Fr.SampleRateIndex];

    // Calculate the total size.
    Fr.FrameSize = elMpegGenerator::CalculateFrameSize(Fr.BitrateIndex, Fr.SampleRate, Fr.Version);
    if (Fr.Padding)
    {
        Fr.FrameSize++;
    }

    // Calculate the number of channels;
    Fr.Channels = (Fr.ChannelMode == CM_MONO) ? 1 : 2;

    // Seek past the header and the CRC.
    m_Input->seekg(Fr.HeaderSize, std::ios_base::cur);

    //VERBOSE("Frame size: " << Fr.FrameSize);
    
    return true;
}

bool elMpegParser::ProcessMpegFrame(elFrame& Fr, elMpegParser::elRawFrameHeader& Hdr)
{
    // Construct our frame.
    Fr.Gr[0].Version = Fr.Gr[1].Version = Hdr.Version;
    Fr.Gr[0].SampleRateIndex = Fr.Gr[1].SampleRateIndex = Hdr.SampleRateIndex;
    Fr.Gr[0].SampleRate = Fr.Gr[1].SampleRate = Hdr.SampleRate;
    Fr.Gr[0].ChannelMode = Fr.Gr[1].ChannelMode = Hdr.ChannelMode;
    Fr.Gr[0].Channels = Fr.Gr[1].Channels = Hdr.Channels;
    Fr.Gr[0].ModeExtension = Fr.Gr[1].ModeExtension = Hdr.ModeExtension;
    Fr.Gr[0].Index = 0;
    Fr.Gr[1].Index = 1;

    for (unsigned int i = 0; i < 2; i++)
    {
        Fr.Gr[i].ChannelInfo.clear();
        
        for (unsigned int j = 0; j < Hdr.Channels; j++)
        {
            Fr.Gr[i].ChannelInfo.push_back(elChannelInfo());
            Fr.Gr[i].ChannelInfo.back().Scfsi = 0;
        }
    }
    
    // Read the frame into memory, without the 4 byte header.
    uint8_t FrameData[2880];
    const std::streamsize FrameToRead = Hdr.FrameSize - Hdr.HeaderSize;
    const unsigned int SideInfoSize = elMpegGenerator::CalculateSideInfoSize(Hdr.Channels, Hdr.Version);
    m_Input->read((char*)FrameData, FrameToRead);

    // A bitstream for parsing the frame in memory.
    bsBitstream IS(FrameData, Hdr.FrameSize - Hdr.HeaderSize);

    // Parse the side info.
    const unsigned int GrCount = Hdr.Version == MV_1 ? 2 : 1;
    unsigned int MainDataStart;
    
    MainDataStart = IS.ReadBits(elMpegGenerator::CalculateMainDataStartBits(Hdr.Version));
    IS.ReadBits(elMpegGenerator::CalculatePrivateBits(Hdr.Channels, Hdr.Version));

    if (Hdr.Version == MV_1)
    {
        for (unsigned int i = 0; i < Hdr.Channels; i++)
        {
            Fr.Gr[1].ChannelInfo[i].Scfsi = IS.ReadBits(4);
        }
    }

    unsigned int DataSize = 0;
    for (unsigned int i = 0; i < GrCount; i++)
    {
        for (unsigned int j = 0; j < Fr.Gr[i].Channels; j++)
        {
            elGranule& Gr = Fr.Gr[i];
            elChannelInfo& Ci = Gr.ChannelInfo[j];
            
            Ci.Size = IS.ReadBits(12);
            //VERBOSE("        Size: " << Ci.Size);
            Ci.SideInfo[0] = IS.ReadBits(32);
            if (Gr.Version == MV_1)
            {
                Ci.SideInfo[1] = IS.ReadBits(47 - 32);
            }
            else
            {
                Ci.SideInfo[1] = IS.ReadBits(51 - 32);
            }

            DataSize += Ci.Size;
        }
    }

    // Convert DataSize to bytes.
    if (DataSize % 8)
    {
        DataSize += 8 - (DataSize % 8);
    }
    DataSize /= 8;

    // The reservoir.
    bsBitstream Res;
    unsigned int ResBitsLeft = MainDataStart * 8;

    if (DataSize && MainDataStart > m_ReservoirUsed)
    {
        throw (elMpegParserException("Bit reservoir underflow. It is either an invalid MP3 file or there is a bug in this program."));
    }

    if (m_ReservoirUsed && MainDataStart)
    {
        //VERBOSEVAR(int(m_ReservoirUsed - MainDataStart));
        Res.SetData(m_Reservoir + (m_ReservoirUsed - MainDataStart), m_ReservoirUsed);
    }

    // Read in the data.
    for (unsigned int i = 0; i < GrCount; i++)
    {
        elGranule& Gr = Fr.Gr[i];
        
        // How many bits are in this channel's data
        unsigned int GrDataSize = 0;
        for (unsigned int j = 0; j < Gr.Channels; j++)
        {
            GrDataSize += Gr.ChannelInfo[j].Size;
        }

        // Round to the nearest byte
        Gr.DataSizeBits = GrDataSize;
        Gr.DataSize = GrDataSize;
        if (GrDataSize % 8)
        {
            Gr.DataSize += 8 - (GrDataSize % 8);
        }
        Gr.DataSize /= 8;

        // Read it in if there is anything
        if (GrDataSize)
        {
            Gr.Data = shared_array<uint8_t>(new uint8_t[Gr.DataSize]);

            bsBitstream OS(Gr.Data.get(), Gr.DataSize);
            while (GrDataSize)
            {
                if (ResBitsLeft > 0)
                {
                    unsigned int BitsToRead = min(min(32, ResBitsLeft), GrDataSize);
                    uint32_t Bits = Res.ReadBits(BitsToRead);
                    OS.WriteBits(Bits, BitsToRead);
                    ResBitsLeft -= BitsToRead;
                    GrDataSize -= BitsToRead;
                }
                else
                {
                    unsigned int BitsToRead = min(32, GrDataSize);
                    uint32_t Bits = IS.ReadBits(BitsToRead);
                    OS.WriteBits(Bits, BitsToRead);
                    GrDataSize -= BitsToRead;
                }
            }
            OS.WriteToNextByte();
        }
        else
        {
            Gr.Data.reset();
        }
    }

    // Calculate the new reservoir size.
    const int OldReservoirUsed = m_ReservoirUsed;
    m_ReservoirUsed = (Hdr.FrameSize - Hdr.HeaderSize - SideInfoSize) - (DataSize - MainDataStart);

    // Okay, at this point if didn't use up all the data from the reservoir, we have
    // to take care of that.
    unsigned int StillInReservoir = ResBitsLeft / 8;
    if (StillInReservoir > 0)
    {
        //VERBOSEVAR(OldReservoirUsed);
        //VERBOSEVAR(StillInReservoir);
        //VERBOSEVAR(OldReservoirUsed - StillInReservoir);
        memmove(m_Reservoir, m_Reservoir + (OldReservoirUsed - StillInReservoir),
                StillInReservoir);
    }

    //VERBOSEVAR(Hdr.FrameSize);
    //VERBOSEVAR(Hdr.HeaderSize);
    //VERBOSEVAR(SideInfoSize);
    //VERBOSEVAR(DataSize);
    //VERBOSEVAR(MainDataStart);

    //VERBOSEVAR(m_ReservoirUsed);

    // Put the bits on the end into the reservoir.
    if (m_ReservoirUsed < 0)
    {
        throw (elMpegParserException("Data size error. It is either an invalid MP3 file or there is a bug in this program."));
    }
    if (m_ReservoirUsed < sizeof(m_Reservoir))
    {
        IS.SeekToNextByte();
        memcpy(m_Reservoir + StillInReservoir, IS.GetData() + IS.Tell() / 8, m_ReservoirUsed - StillInReservoir);
    }

    // Make sure this frame actually has data
    if (DataSize < 1)
    {
        VERBOSE("Skipped empty frame");
        //VERBOSE("");
        return true;
    }

    // Set used.
    if (Hdr.Version == MV_1)
    {
        Fr.Gr[0].Used = Fr.Gr[1].Used = true;
    }
    else
    {
        Fr.Gr[0].Used = true;
        Fr.Gr[1].Used = false;
    }
    //VERBOSE("-------------next frame---------------");
    return true;
}

elMpegParserException::elMpegParserException(const std::string& What) throw() :
    m_What(What)
{
    return;
}

elMpegParserException::~elMpegParserException() throw()
{
    return;
}

const char* elMpegParserException::what() const throw()
{
    return m_What.c_str();
}
