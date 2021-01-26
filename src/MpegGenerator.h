/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "Parser.h"

#define MAX_MPEG_FRAME_BUFFER (144 * 1000 * 320 / 32000 * 2)

class bsBitstream;
class elBlock;
class elMpegOutputStream;
class elPcmOutputStream;

class elMpegGenerator
{
public:
    elMpegGenerator();
    ~elMpegGenerator();

    /// Clears all data from this object and restores it to an initial state;
    void Clear();

    /// Scan the first block for information about the bitstream.
    bool Initialize(const elBlock& FirstBlock, shared_ptr<elParser> Parser);

    /// Get the total number of streams.
    unsigned int GetStreamCount() const;

    /// Get the total number of uncompressed sample frames that were ignored.
    unsigned long GetUncSampleFrameCount() const;

    /// Get the total number of sample frames that were ignored.
    unsigned long GetSampleFrameCount() const;

    /// Get the sample rate of a stream.
    unsigned int GetSampleRate(unsigned int StreamIndex = 0) const;

    /// Get the number of channels in a stream.
    unsigned int GetChannels(unsigned int StreamIndex = 0) const;

    /// Parses the block and adds it to the internal output buffer. Remember to call this on the first frame.
    void ParseBlock(const elBlock& Block);

    /// Call this when all the blocks have been read in.
    void DoneParsingBlocks();

    /// Create an MPEG stream from the output frames.
    shared_ptr<elMpegOutputStream> CreateMpegStream(unsigned int StreamIndex = 0) const;

    /// Create a PCM stream from the output frames
    shared_ptr<elPcmOutputStream> CreatePcmStream(unsigned int StreamIndex = 0) const;

    /// Get the total number of frames in the output.
    unsigned int GetFrameCount(unsigned int StreamIndex = 0) const;

    /// Reads a frame from the output.
    unsigned int ReadFrame(uint8_t* Buffer, unsigned int BufferSize, unsigned int Index, unsigned int StreamIndex = 0) const;

    /// Gets uncompressed samples from the output.
    const elUncompressedSampleFrames& ReadUncSamples(unsigned int Granule, unsigned int Index, unsigned int StreamIndex = 0) const;

    
protected:
    /// Information about each stream.
    struct elStreamInfo
    {
        unsigned int SampleRate;
        unsigned char Channels;
    };

    /// A place to store a decoded MPEG audio frame.
    struct elMpegFrame
    {
        elMpegFrame() : HeaderSize(0), Data(new uint8_t[MAX_MPEG_FRAME_BUFFER]),
            Used(0), Size(0), UsedFromPrevious(0), UsedByNext(0), Version(0),
            SampleRate(0), Channels(0) {};

        unsigned int HeaderSize;
        shared_array<uint8_t> Data;
        unsigned int Used;
        unsigned int Size;
        unsigned int UsedFromPrevious;
        unsigned int UsedByNext;

        unsigned int Version;
        unsigned int SampleRate;
        unsigned int Channels;

        elUncompressedSampleFrames UncompA;
        elUncompressedSampleFrames UncompB;
    };

    typedef std::vector<elStreamInfo> elStreamInfoVector;
    typedef std::vector<elMpegFrame> elMpegStream;
    typedef std::vector<elMpegStream> elMpegStreamVector;

    void ReadBlockData(elStreamVector& Streams, bsBitstream& IS);
    void ConstructMpegVbrFrame(const elGranule* Granule, elMpegFrame& Out, unsigned int Frames, unsigned int DataSize);
    void ConstructMpegFrame(const elFrame& Fr, bsBitstream& IS, elMpegFrame& Out);
    void ConstructMpegFrameV1(const elFrame& Fr, bsBitstream& IS, elMpegFrame& Out);
    void ConstructMpegFrameV2(const elFrame& Fr, bsBitstream& IS, elMpegFrame& Out);
public:
    static unsigned int EstimateBitrateIndex(unsigned int FrameUsed, unsigned int SampleRate, unsigned int Version);
    static unsigned int CalculateFrameSize(unsigned int BitrateIndex, unsigned int SampleRate, unsigned int Version);
    static unsigned int CalculateSideInfoSize(unsigned int Channels, unsigned int Version);
    static unsigned int CalculatePrivateBits(unsigned int Channels, unsigned int Version);
    static unsigned int CalculateMainDataStartBits(unsigned int Version);
protected:
    void WriteFields(elMpegFrame& Frame, unsigned int NewBitrateIndex, unsigned int NewUsedFromPrev) const;

    void Print(const elGranule& Gr, const std::string& Indent);
    void Print(const elStreamVector& Streams);

    /// The EALayer3 parser we are using.
    shared_ptr<elParser> m_Parser;

    /// The data read from the file.
    elStreamVector m_Streams;

    /// The current frame number for debugging purposes.
    unsigned long m_CurrentFrame;

    /// The number of uncompressed sample frames encountered from all streams.
    unsigned long m_UncompressedSampleFrames;

    /// The number of sample frames encountered from all streams.
    unsigned long m_SampleFrames;

    /// Holds the information about each stream.
    elStreamInfoVector m_StreamInfo;

    /// Are we done parsing blocks yet?
    bool m_DoneParsingBlocks;

    /// The iterator for the parser.
    unsigned int m_CurMpegFrame;

    /// The iterator for the output.
    unsigned int m_CurOutputMpegFrame;

    /// Hold all of the outputted MPEG audio frames for each stream.
    elMpegStreamVector m_Outputs;
};

class elMpegGeneratorException : public std::exception
{
public:
    elMpegGeneratorException(const std::string& What) throw();
    virtual ~elMpegGeneratorException() throw();
    virtual const char* what() const throw();

protected:
    std::string m_What;
};
