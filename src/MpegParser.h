/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"

struct elFrame;
struct elChannelInfo;

class elMpegParser
{
public:
    elMpegParser();
    ~elMpegParser();

    /// Initialize the parser with a pointer to the input stream.
    void Initialize(std::istream* Input);

    /// Read the next frame from the file, returns true if there is one, false otherwise.
    bool ReadFrame(elFrame& Frame);

    /// Are there any frames left?
    bool FramesLeft() const;
    
protected:
    /// A raw MPEG frame
    struct elRawFrameHeader
    {
        unsigned int Version;
        bool Crc;
        unsigned int BitrateIndex;
        unsigned int SampleRateIndex;
        bool Padding;
        unsigned int ChannelMode;
        unsigned int ModeExtension;

        unsigned int HeaderSize;
        unsigned int SampleRate;
        unsigned int FrameSize;
        unsigned int Channels;
    };
    
    /// Skip over a ID3 tag.
    void SkipID3Tag(uint8_t FrameHeader[10]);

    /// Process the MPEG frame header.
    bool ProcessFrameHeader(elRawFrameHeader& Frame, uint8_t FrameHeader[10]);

    /// Process an actual frame.
    bool ProcessMpegFrame(elFrame& Fr, elRawFrameHeader& Hdr);
    
    std::istream* m_Input;

    uint8_t m_Reservoir[2880];
    int m_ReservoirUsed;
};

class elMpegParserException : public std::exception
{
public:
    elMpegParserException(const std::string& What) throw();
    virtual ~elMpegParserException() throw();
    virtual const char* what() const throw();

protected:
    std::string m_What;
};
