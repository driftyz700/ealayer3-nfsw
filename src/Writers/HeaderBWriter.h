/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include "../BlockWriter.h"

class elHeaderBWriter : public elBlockWriter
{
public:
    elHeaderBWriter();
    virtual ~elHeaderBWriter();

    /**
     * Initialize the block writer with a pointer to an output stream.
     */
    virtual void Initialize(std::ostream* Output);

    /**
     * Write the next block to the output file.
     */
    virtual void WriteNextBlock(const elBlock& Block, bool LastBlock);

protected:
    virtual void WriteHeader(const elBlock& Block);
    virtual void WriteEnder();
    
    bool m_WrittenHeader;
};
