/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

void PrepareWaveHeader(std::ostream& Output);
void WriteWaveHeader(std::ostream& Output, unsigned long SampleRate, \
                       unsigned char BitsPerSample, unsigned char Channels, \
                       unsigned long NumberSamples);
