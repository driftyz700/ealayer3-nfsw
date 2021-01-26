/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "PcmOutputStream.h"
#include "MpegGenerator.h"

#include <mpg123.h>

#ifndef min
#define min(a, b) ( (a) < (b) ? (a) : (b) )
#endif // min


elPcmOutputStream::elPcmOutputStream(const elMpegGenerator& Gen, unsigned int StreamIndex):
    elOutputStream(Gen, StreamIndex),
    m_Decoder(NULL),
    m_SamplesLeft(0)
{
    // Initialize the decoder
    m_Decoder = mpg123_new(NULL, NULL);
    mpg123_open_feed(m_Decoder);
    mpg123_param(m_Decoder, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
    m_SamplesLeft = m_Gen.GetSampleFrameCount() * GetChannels();
    return;
}

elPcmOutputStream::~elPcmOutputStream()
{
    if (m_Decoder)
    {
        mpg123_delete(m_Decoder);
        m_Decoder = NULL;
    }
    return;
}

unsigned int elPcmOutputStream::Read(short int* Buffer, unsigned int BufferSamples)
{
    // Check to make sure that we have something to decode
    if (!m_Decoder)
    {
        m_Eos = true;
        return 0;
    }

    // See if we can decode anything
    int Result;
    off_t DecoderFrameIndex;
    unsigned char* InternalBuffer;
    size_t Done;
    Result = mpg123_decode_frame(m_Decoder, &DecoderFrameIndex, &InternalBuffer, &Done);

    // If we need a new format do that and try it again
    if (Result == MPG123_NEW_FORMAT)
    {
        long Rate;
        int Channels;
        int Encoding;
        mpg123_getformat(m_Decoder, &Rate, &Channels, &Encoding);
        mpg123_format_none(m_Decoder);
        mpg123_format(m_Decoder, GetSampleRate(), GetChannels(), MPG123_ENC_SIGNED_16);
        Result = mpg123_decode_frame(m_Decoder, &DecoderFrameIndex, &InternalBuffer, &Done);
    }

    // Handle the return value
    if (Result == MPG123_NEED_MORE)
    {
        if (m_CurrentFrame >= m_Gen.GetFrameCount(m_StreamIndex))
        {
            // We don't have any more
            m_Eos = true;
            return 0;
        }
        else
        {
            FeedNextFrame();
            return Read(Buffer, BufferSamples);
        }
    }
    else if (Result == MPG123_NEW_FORMAT)
    {
        // Err... can this happen?
        m_Eos = true;
        return 0;
    }
    else if (Result == MPG123_OK)
    {
    }
    else
    {
        throw (elMpg123Exception(Result));
    }

    // Now that we have the buffer
    unsigned long Samples = Done / sizeof(short);
    if (Samples > BufferSamples)
    {
        return 0;
    }
    memcpy(Buffer, InternalBuffer, Done);

    // Add the uncompressed samples
    unsigned int NewSamples;
    NewSamples = FixupOutFrame(Buffer, Samples, DecoderFrameIndex);
    Samples = min(NewSamples, m_SamplesLeft);
    m_SamplesLeft -= Samples;
    return Samples;
}

unsigned int elPcmOutputStream::RecommendBufferSize()
{
    return (unsigned int)mpg123_safe_buffer();
}

unsigned int elPcmOutputStream::FeedNextFrame()
{
    // A place to store the current compressed frame
    static uint8_t MpegFrame[MAX_MPEG_FRAME_BUFFER];
    unsigned int Bytes = 0;
    if (m_CurrentFrame < m_Gen.GetFrameCount(m_StreamIndex))
    {
        Bytes = m_Gen.ReadFrame(MpegFrame, sizeof(MpegFrame), m_CurrentFrame++, m_StreamIndex);
    }

    // Now feed it to the decoder
    if (Bytes > 0)
    {
        int Result;
        Result = mpg123_feed(m_Decoder, MpegFrame, Bytes);
    }
    return Bytes;
}

unsigned int elPcmOutputStream::FixupOutFrame(short* Buffer, unsigned int BufferSamples, unsigned int FrameIndex)
{
    if (FrameIndex == 0)
    {
        // Discard the VBR info frame
        return 0;
    }
    if (BufferSamples < 1)
    {
        return 0;
    }

    // The uncompressed samples
    const elUncompressedSampleFrames& GrA = m_Gen.ReadUncSamples(0, FrameIndex, m_StreamIndex);
    const elUncompressedSampleFrames& GrB = m_Gen.ReadUncSamples(1, FrameIndex, m_StreamIndex);
    const unsigned int GrOffsetA = 0;
    const unsigned int GrOffsetB = 576 * GetChannels();
    unsigned int ToCopy;

    // If there are no uncompressed samples exit early
    if (GrA.Count == 0 && GrB.Count == 0)
    {
        return BufferSamples;
    }

    // ====== This function needs work! ======
    // So far I haven't found out exactly how the uncompressed samples work, but I know just enough to get by.

    // If we have a full granule go ahead and replace it
    if (GrA.Count == 576)
    {
        ToCopy = min(GrA.Count * GetChannels(), BufferSamples);
        memcpy(Buffer, GrA.Data.get(), ToCopy * sizeof(short));
    }
    if (GrB.Count == 576)
    {
        ToCopy = min(GrB.Count * GetChannels(), (long)BufferSamples - GrOffsetB);
        memcpy(Buffer + GrOffsetB, GrB.Data.get(), ToCopy * sizeof(short));
    }
    if (GrA.Count == 1152)
    {
        ToCopy = min(GrA.Count * GetChannels(), BufferSamples);
        memcpy(Buffer, GrA.Data.get(), ToCopy * sizeof(short));
    }
    if (GrB.Count == 1152)
    {
        ToCopy = min(GrB.Count * GetChannels(), BufferSamples);
        memcpy(Buffer, GrB.Data.get(), ToCopy * sizeof(short));
    }

    // If this is the first frame replace it
    if (FrameIndex == 1)
    {
        if (GrA.Count && GrA.Count < 576)
        {
            ToCopy = GrA.Count * GetChannels();
            memcpy(Buffer, GrA.Data.get(), ToCopy * sizeof(short));
            return GrA.Count * GetChannels();
        }
        if (GrB.Count && GrB.Count < 576)
        {
            ToCopy = GrB.Count * GetChannels();
            memcpy(Buffer, GrB.Data.get(), ToCopy * sizeof(short));
            return ToCopy;
        }
    }

    VERBOSE("Skipping " << (GrA.Count + GrB.Count) << " uncompressed samples.");
    return BufferSamples;
}

elMpg123Exception::elMpg123Exception(int ErrorCode) throw():
    m_ErrorCode(ErrorCode)
{
    return;
}

elMpg123Exception::~elMpg123Exception() throw()
{
    return;
}

const char* elMpg123Exception::what() const throw()
{
    return mpg123_plain_strerror(m_ErrorCode);
}

// Initialize the decoder
struct elMpg123Initializer
{
    inline elMpg123Initializer()
    {
        mpg123_init();
    }
    inline ~elMpg123Initializer()
    {
        mpg123_exit();
    }
};

elMpg123Initializer g_RealMpg123Initializer;
