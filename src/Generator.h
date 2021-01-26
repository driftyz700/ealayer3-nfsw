/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"

struct elFrame;
struct elGranule;
class elBlock;
class bsBitstream;

/**
 * An EA Layer 3 generator.
 */
class elGenerator
{
public:
    elGenerator();
    virtual ~elGenerator();

    /**
     * Initialize the generator.
     */
    virtual void Initialize();

    /**
     * Push a frame onto the queue. For each frame this should be called once
     * for each stream, and after that Generate should be called to create the
     * resulting block.
     */
    virtual void AddFrameFromStream(const elFrame& Fr);

    /**
     * Clear the frame queue.
     */
    virtual void Clear();

    /**
     * Generate the resulting block. Takes all of the frames pushed so far, and
     * then clears the queue.
     * @param Keep If non-zero then Generate will not discard the existing contents
     *             of the frame, but rather it respects the data that is already
     *             there, starting Keep bytes into the data.
     * @returns    Will return true if data was written to the block, false otherwise.
     */
    virtual bool Generate(elBlock& Block, bool first, unsigned int Keep = 0);

protected:
    /**
     * Write a granule and uncompressed samples if there are any to the output
     * bitstream.
     */
    virtual void WriteGranuleWithUncSamples(bsBitstream& OS, const elGranule& Gr);

    /**
     * Write a compressed granule to the output bitstream.
     */
    virtual void WriteGranule(bsBitstream& OS, const elGranule& Gr);

    /**
     * Write uncompressed samples to the output bitstream.
     */
    virtual void WriteUncSamples(bsBitstream& OS, const elGranule& Gr);

    
    std::vector<elFrame> m_Streams;
};
