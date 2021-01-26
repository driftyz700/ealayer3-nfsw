/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include <assert.h>

#ifndef NULL
#define NULL 0
#endif // NULL

#ifndef min
#define min(a, b) ( (a) < (b) ? (a) : (b) )
#endif // min

#define BITMASK(_count) uint32_t(0xFFFFFFFF >> (32 - _count))


class bsBitstream
{
public:
    inline bsBitstream() :
        m_DataStart(NULL),
        m_DataCur(NULL),
        m_DataEnd(NULL),
        m_BitsInto(0),
        m_BitBufferUsed(0)
    {
        return;
    }
    
    inline bsBitstream(uint8_t* Data, unsigned long SizeInBytes) :
        m_DataStart(NULL),
        m_DataCur(NULL),
        m_DataEnd(NULL),
        m_BitsInto(0),
        m_BitBufferUsed(0)
    {
        SetData(Data, SizeInBytes);
        return;
    }
    
    inline ~bsBitstream()
    {
        return;
    }

    inline void SetData(uint8_t* Data, unsigned long SizeInBytes)
    {
        assert(Data);
        assert(SizeInBytes);
        
        m_DataStart = Data;
        m_DataCur = Data;
        m_DataEnd = Data + SizeInBytes;
        m_BitsInto = 0;
        m_BitBufferUsed = 0;
        FillBitBuffer();
        return;
    }
    
    inline uint8_t* GetData() const
    {
        return m_DataStart;
    }

    inline uint8_t* GetDataAtCurrentOffset() const
    {
        return m_DataCur;
    }

    inline uint8_t* GetDataAtEnd() const
    {
        return m_DataEnd;
    }

    inline unsigned long GetSizeInBytes() const
    {
        return m_DataEnd - m_DataStart;
    }

    inline unsigned int ReadBit()
    {
        return ReadBits(1);
    }

    inline unsigned int ReadBits(unsigned int Count)
    {
        assert(Count <= 32);
        assert(m_BitBufferUsed >= 0 && m_BitBufferUsed <= 32);

        // Make sure we don't read past the end of the stream
        Count = min(Count, GetCountBitsLeft());
        if (!Count)
        {
            return 0;
        }

        // Make sure there are enough bits in the buffer
        if (m_BitBufferUsed < Count)
        {
            FillBitBuffer();
        }

        // Get the bits
        unsigned int Bits;
        Bits = (m_BitBuffer >> (32 - Count)) & BITMASK(Count);
        UpdateBitBuffer(Count);

        // Update the current position
        Count += m_BitsInto;
        m_DataCur = m_DataCur + Count / 8;
        m_BitsInto = Count % 8;

        assert(m_DataCur <= m_DataEnd);
        return Bits;
    }

    inline void WriteBit(unsigned int Bit)
    {
        WriteBits(Bit, 1);
        return;
    }

    inline void WriteBits(unsigned int Bits, unsigned int Count)
    {
        assert(m_DataCur);
        assert(Count <= 32);
        assert(m_BitBufferUsed >= 0 && m_BitBufferUsed <= 32);

        // Make sure we don't write past the end of the stream
        Count = min(Count, GetCountBitsLeft());
        if (!Count)
        {
            return;
        }

        // Empty the bit buffer, we don't need it
        m_BitBufferUsed = 0;

        // Set the bits
        unsigned int EndBit;
        unsigned int KeepBitsBegin;
        unsigned int KeepBitsEnd;
        bool NeedEndBit;
        
        EndBit = m_BitsInto + Count;
        NeedEndBit = (EndBit % 8) > 0 ? true : false;
        Bits <<= 32 - Count;

        KeepBitsBegin = m_DataCur[0] & (0xFF << (8 - m_BitsInto));
        if (NeedEndBit)
        {
            KeepBitsEnd = m_DataCur[EndBit / 8] & (0xFF >> (EndBit % 8));
        }

        unsigned int Bytes = 0;
        unsigned int BitPos = m_BitsInto;
        for (unsigned int BitsLeft = Count; BitsLeft > 0;)
        {
            unsigned int BitsWritten = min(8 - BitPos, BitsLeft);
            m_DataCur[Bytes] = Bits >> (24 + BitPos);
            Bits <<= BitsWritten;
            BitsLeft -= BitsWritten;
            BitPos = 0;
            Bytes++;
        }

        m_DataCur[0] |= KeepBitsBegin;
        if (NeedEndBit)
        {
            m_DataCur[EndBit / 8] |= KeepBitsEnd;
        }

        // Code used for testing the integrity of a write
        /*unsigned int OldBitsInto = m_BitsInto;
        unsigned int ActualBits = ReadBits(Count);
        if (OldBits != ActualBits)
        {
            std::cout << "Integrity failure at " << Tell() - Count << ", m_BitsInto: " << (int)OldBitsInto << ", Count: " << Count << std::endl;
            std::cout << "Is: " << (void*)ActualBits << ", should be: " << (void*)OldBits << std::endl;
            assert (0);
        }
        else
        {
            std::cout << "Correct at " << Tell() - Count << ", m_BitsInto: " << (int)OldBitsInto << ", Count: " << Count << std::endl;
        }*/

        // Update the current position
        Count += m_BitsInto;
        m_DataCur = m_DataCur + Count / 8;
        m_BitsInto = Count % 8;

        assert(m_DataCur <= m_DataEnd);
        return;
    }

    template <typename T> inline T ReadAligned32BE()
    {
        // Align to nearest byte and make sure there's enough left
        SeekToNextByte();
        if (GetCountBitsLeft() < 32)
        {
            SeekToEnd();
            return 0;
        }

        // Get the data
        T Data;
        Data = static_cast<T>(m_DataCur[0] << 24 | m_DataCur[1] << 16 | m_DataCur[2] << 8 | m_DataCur[3]);

        // Update the current position
        m_DataCur += 4;
        m_BitBufferUsed = 0;
        return Data;
    }

    template <typename T> inline T ReadAligned16BE()
    {
        // Align to nearest byte and make sure there's enough left
        SeekToNextByte();
        if (GetCountBitsLeft() < 16)
        {
            SeekToEnd();
            return 0;
        }

        // Get the data
        T Data;
        Data = static_cast<T>(m_DataCur[0] << 8 | m_DataCur[1]);

        // Update the current position
        m_DataCur += 2;
        m_BitBufferUsed = 0;
        return Data;
    }

    template <typename T> inline T ReadAligned8()
    {
        // Align to nearest byte and make sure there's enough left
        SeekToNextByte();
        if (GetCountBitsLeft() < 8)
        {
            SeekToEnd();
            return 0;
        }

        // Get the data
        T Data;
        Data = static_cast<T>(m_DataCur[0]);

        // Update the current position
        m_DataCur++;
        m_BitBufferUsed = 0;
        return Data;
    }

    template <typename T> inline void WriteAligned32BE(T Value)
    {
        // Align to nearest byte and make sure there's enough left
        WriteToNextByte();
        if (GetCountBitsLeft() < 32)
        {
            SeekToEnd();
            return;
        }

        // Get the data
        m_DataCur[0] = (Value >> 24) & 0xFF;
        m_DataCur[1] = (Value >> 16) & 0xFF;
        m_DataCur[2] = (Value >> 8) & 0xFF;
        m_DataCur[3] = Value & 0xFF;

        // Update the current position
        m_DataCur += 4;
        m_BitBufferUsed = 0;
        return;
    }

    template <typename T> inline void WriteAligned16BE(T Value)
    {
        // Align to nearest byte and make sure there's enough left
        WriteToNextByte();
        if (GetCountBitsLeft() < 16)
        {
            SeekToEnd();
            return;
        }

        // Get the data
        m_DataCur[0] = (Value >> 8) & 0xFF;
        m_DataCur[1] = Value & 0xFF;

        // Update the current position
        m_DataCur += 2;
        m_BitBufferUsed = 0;
        return;
    }

    template <typename T> inline void WriteAligned8(T Value)
    {
        // Align to nearest byte and make sure there's enough left
        WriteToNextByte();
        if (GetCountBitsLeft() < 8)
        {
            SeekToEnd();
            return;
        }

        // Get the data
        m_DataCur[0] = Value & 0xFF;

        // Update the current position
        m_DataCur++;
        m_BitBufferUsed = 0;
        return;
    }

    inline unsigned long GetCountBitsLeft() const
    {
        assert(m_BitsInto < 8);
        assert(m_DataEnd != m_DataCur || m_BitsInto == 0);
        
        return (m_DataEnd - m_DataCur) * 8 - m_BitsInto;
    }

    inline unsigned long Tell() const
    {
        assert(m_BitsInto < 8);
        assert(m_DataEnd != m_DataCur || m_BitsInto == 0);
        
        return (m_DataCur - m_DataStart) * 8 + m_BitsInto;
    }

    inline bool Eos() const
    {
        assert(m_DataCur <= m_DataEnd);
        
        return m_DataCur == m_DataEnd;
    }

    inline void SeekAbsolute(unsigned long NewOffset)
    {
        assert(m_DataStart);
        assert(m_BitsInto < 8);

        // Calculate a new offset
        uint8_t* NewData;
        NewData = m_DataStart + NewOffset / 8;

        // Should be an impossiblity but we'll check anyways
        assert(NewData >= m_DataStart);

        // If it's past the end set it to the end
        if (NewData >= m_DataEnd)
        {
            m_DataCur = m_DataEnd;
            m_BitsInto = 0;
            m_BitBufferUsed = 0;
        }
        else
        {
            m_DataCur = NewData;
            m_BitsInto = NewOffset % 8;
            m_BitBufferUsed = 0;
        }
        return;
    }

    inline void SeekRelative(long Offset)
    {
        SeekAbsolute(Tell() + Offset);
        return;
    }

    inline void SeekToEnd()
    {
        assert(m_DataEnd);
        
        m_DataCur = m_DataEnd;
        m_BitsInto = 0;
        m_BitBufferUsed = 0;
        return;
    }

    inline void Rewind()
    {
        SeekAbsolute(0);
        return;
    }

    inline void SeekToNextByte()
    {
        assert(m_DataCur);
        
        if (Eos())
        {
            return;
        }

        if (m_BitsInto)
        {
            m_DataCur++;
            UpdateBitBuffer(8 - m_BitsInto);
            m_BitsInto = 0;
            m_BitBufferUsed = 0;
        }
        return;
    }

    inline void WriteToNextByte()
    {
        if (m_BitsInto)
        {
            WriteBits(0, 8 - m_BitsInto);
        }
        return;
    }

protected:
    inline void FillBitBuffer()
    {
        assert(m_DataCur);
        
        m_BitBufferUsed = min(32, GetCountBitsLeft());
        if (m_DataEnd - m_DataCur > 4)
        {
            m_BitBuffer =
                m_DataCur[0] << (24 + m_BitsInto) |
                m_DataCur[1] << (16 + m_BitsInto) |
                m_DataCur[2] << (8 + m_BitsInto) |
                m_DataCur[3] << m_BitsInto |
                m_DataCur[4] >> (8 - m_BitsInto);
        }
        else if (m_DataEnd - m_DataCur > 3)
        {
            m_BitBuffer =
                m_DataCur[0] << (24 + m_BitsInto) |
                m_DataCur[1] << (16 + m_BitsInto) |
                m_DataCur[2] << (8 + m_BitsInto) |
                m_DataCur[3] << m_BitsInto;
        }
        else if (m_DataEnd - m_DataCur > 2)
        {
            m_BitBuffer =
                m_DataCur[0] << (24 + m_BitsInto) |
                m_DataCur[1] << (16 + m_BitsInto) |
                m_DataCur[2] << (8 + m_BitsInto);
        }
        else if (m_DataEnd - m_DataCur > 1)
        {
            m_BitBuffer =
                m_DataCur[0] << (24 + m_BitsInto) |
                m_DataCur[1] << (16 + m_BitsInto);
        }
        else if (m_DataEnd - m_DataCur > 0)
        {
            m_BitBuffer =
                m_DataCur[0] << (24 + m_BitsInto);
        }
        return;
    }

    inline void UpdateBitBuffer(unsigned long Count)
    {
        Count = min(Count, m_BitBufferUsed);
        m_BitBuffer <<= Count;
        m_BitBufferUsed -= Count;
        return;
    }

    
    uint8_t* m_DataStart;
    uint8_t* m_DataCur;
    uint8_t* m_DataEnd;
    unsigned int m_BitsInto;

    uint32_t m_BitBuffer;
    unsigned int m_BitBufferUsed;
};
