/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "BlockWriter.h"

elBlockWriter::elBlockWriter() :
    m_Output(NULL)
{
    return;
}

elBlockWriter::~elBlockWriter()
{
    return;
}

void elBlockWriter::Initialize(std::ostream* Output)
{
    if (!Output)
    {
        throw (std::exception());
    }
    m_Output = Output;
    return;
}
