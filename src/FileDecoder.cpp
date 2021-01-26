/*
 *  EA Layer 3 Extractor/Decoder
 *  Copyright (C) 2011, Ben Moench.
 *  See License.txt
 */

#include "Internal.h"
#include "FileDecoder.h"

#include "AllFormats.h"
#include "Parsers/ParserVersion6.h"
#include "BlockLoader.h"
#include "MpegGenerator.h"
#include "MpegOutputStream.h"
#include "PcmOutputStream.h"
#include "WaveWriter.h"

#include <fstream>
#include <stdexcept>
#include <boost/format.hpp>

using boost::format;
using std::runtime_error;


static void _SeparateFilename(const std::string& Filename, std::string& PathAndName, std::string& Ext)
{
    PathAndName = Filename;
    Ext = "";
    
    for (unsigned int i = Filename.length(); i > 0; i--)
    {
        if (Filename.at(i - 1) == '.')
        {
            PathAndName = Filename.substr(0, i - 1);
            Ext = Filename.substr(i - 1);
            break;
        }
    }
    
    return;
}


elFileDecoder::elFileDecoder() :
    inputFilename(""),
    inputOffset(0),
    inputStream(-1),
    inputParser(P_AUTO),
    outputFilename(""),
    outputFormat(F_AUTO)
{
    return;
}


elFileDecoder::~elFileDecoder()
{
    return;
}


void elFileDecoder::SetInput(const std::string& filename, std::streamoff offset)
{
    this->inputFilename = filename;
    this->inputOffset = offset;
    return;
}


const std::string& elFileDecoder::GetInput() const
{
    return this->inputFilename;
}


void elFileDecoder::SetStream(int stream)
{
    this->inputStream = stream;
    return;
}


int elFileDecoder::GetStream() const
{
    return this->inputStream;
}


void elFileDecoder::SetParser(elFileDecoder::Parser parser)
{
    this->inputParser = parser;
    return;
}


elFileDecoder::Parser elFileDecoder::GetParser() const
{
    return this->inputParser;
}


void elFileDecoder::SetOutput(const std::string& baseFilename, elFileDecoder::Format format)
{
    this->outputFilename = baseFilename;
    this->outputFormat = format;
    return;
}


const std::string& elFileDecoder::GetOutputFilename() const
{
    return this->outputFilename;
}


elFileDecoder::Format elFileDecoder::GetOutputFormat() const
{
    return this->outputFormat;
}


void elFileDecoder::Process()
{
    // First, make sure we've got some kind of output format
    if (outputFormat == F_AUTO)
    {
        // Autodectect based on extension
    }
    
    // Open the input file
    std::ifstream input;
    input.open(inputFilename.c_str(), std::ios_base::in | std::ios_base::binary);
    if (!input.is_open())
    {
        throw (runtime_error("Could not open input file '" + inputFilename + "'."));
    }
    
    // Get file size
    std::streampos fileSize;
    input.seekg(0, std::ios_base::end);
    fileSize = input.tellg();
    input.seekg(inputOffset);
    
    // Process the first part
    currentPart = 0;
    ProcessPart(input);
    
    // Are there more parts?
    while (!input.eof() && (4 + input.tellg()) < fileSize)
    {
        currentPart++;
        
        VERBOSE("Trying to process part " << (currentPart + 1));
        try
        {
            ProcessPart(input);
        }
        catch (std::exception& E)
        {
            VERBOSE("Exception processing further part: " << E.what());
            break;
        }
        catch (...)
        {
            VERBOSE("Crash or something else processing further part.");
            break;
        }
    }
    
    VERBOSE("Done.");
    return;
}


void elFileDecoder::ProcessPart(std::ifstream& input)
{
    // Determine the input's file type here
    elBlockLoaderSelector loader;
    if (!loader.Initialize(&input))
    {
        throw (runtime_error("The input is not in a readable file format."));
    }
    
    // Grab the first block
    elBlock firstBlock;
    if (!loader.ReadNextBlock(firstBlock))
    {
        throw (runtime_error("The first block could not be read from the input."));
    }
    
    // Create the parser.
    shared_ptr<elParser> parser;
    switch (inputParser)
    {
        case P_VERSION5:
            parser = make_shared<elParser>();
            break;
            
        case P_VERSION6:
            parser = make_shared<elParserVersion6>();
            break;
            
        case P_AUTO:
        default:
            parser = loader.CreateParser();
            break;
    }
    
    // Add the first block to the generator.
    elMpegGenerator gen;
    if (!gen.Initialize(firstBlock, parser))
    {
        throw (runtime_error("The EALayer3 parser could not be initialized (the bitstream format is not readable)."));
    }
    
    // Check the stream count.
    if (inputStream != -1 && inputStream >= gen.GetStreamCount())
    {
        throw (runtime_error((format("The stream index (%i) exceeds the total number of streams (%i).") %
            (inputStream + 1) % gen.GetStreamCount()).str()));
    }
    
    // Load in the file
    VERBOSE("Parsing blocks...");
    gen.ParseBlock(firstBlock);
    
    while (true)
    {
        elBlock block;
        if (!loader.ReadNextBlock(block))
        {
            break;
        }
        
        gen.ParseBlock(block);
    }
    
    gen.DoneParsingBlocks();
    
    // Write it out in the preferred output format
    VERBOSE("Writing output file...");
    
    if (outputFormat == F_AUTO)
    {
        AutoSetOutputFormat();
    }

    if (inputStream == -1)
    {
        if (outputFormat == F_MULTI_WAVE)
        {
            WriteMultiWave(gen);
        }
        else
        {
            WriteAllStreams(gen);
        }
    }
    else
    {
        WriteSingleStream(gen);
    }
    return;
}


void elFileDecoder::AutoSetOutputFormat()
{
    VERBOSE("Auto setting the output format");
    
    // Autodetect the output format from filename
    if (outputFilename.empty())
    {
        outputFormat = F_MP3;
    }
    else
    {
        std::string pathAndName;
        std::string ext;
        _SeparateFilename(outputFilename, pathAndName, ext);
        
        for (unsigned int i = 0; i < ext.length(); i++)
        {
            ext[i] = tolower(ext[i]);
        }
        
        if (ext == ".mp3")
        {
            outputFormat = F_MP3;
        }
        else if (ext == ".wav")
        {
            outputFormat = F_WAVE;
        }
        else
        {
            outputFormat = F_MP3;
        }
    }
    return;
}


std::string elFileDecoder::GenOutputFilename(const std::string& append) const
{
    std::string pathAndName;
    std::string ext;
    
    if (outputFilename.empty())
    {
        _SeparateFilename(inputFilename, pathAndName, ext);
        
        switch (outputFormat)
        {
            case F_MP3:
                ext = ".mp3";
                break;
            case F_WAVE:
            case F_MULTI_WAVE:
                ext = ".wav";
                break;
            case F_AUTO:
            default:
                ext = ".mp3";
                break;
        }
    }
    else
    {
        _SeparateFilename(outputFilename, pathAndName, ext);
    }
    
    return pathAndName + append + ext;
}


void elFileDecoder::WriteSingleStream(elMpegGenerator& gen)
{
    // Get output file name
    std::string filename;
    if (currentPart == 0)
    {
        filename = GenOutputFilename("");
    }
    else
    {
        filename = GenOutputFilename((format("_part%i") % (currentPart + 1)).str());
    }
    
    // Open it and write it
    std::ofstream outFile;
    VERBOSE("Output file: " << filename);
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::binary);
    if (!outFile.is_open())
    {
        throw (runtime_error("Could not open output file '" + filename + "'."));
    }
    
    WriteMp3OrWave(outFile, gen, inputStream);
}


void elFileDecoder::WriteAllStreams(elMpegGenerator& gen)
{
    const int count = gen.GetStreamCount();
    for (unsigned int i = 0; i < count; i++)
    {
        // Get output file name
        std::string filename;
        if (currentPart == 0)
        {
            if (count == 1)
            {
                filename = GenOutputFilename("");
            }
            else
            {
                filename = GenOutputFilename((format("_%i") % (i + 1)).str());
            }
        }
        else
        {
            if (count == 1)
            {
                filename = GenOutputFilename((format("_part%i") % (currentPart + 1)).str());
            }
            else
            {
                filename = GenOutputFilename((format("_%ipart%i") % (i + 1) % (currentPart + 1)).str());
            }
        }
        
        // Open it and write it
        std::ofstream outFile;
        VERBOSE("Output file: " << filename);
        outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::binary);
        if (!outFile.is_open())
        {
            throw (runtime_error("Could not open output file '" + filename + "'."));
        }
        
        WriteMp3OrWave(outFile, gen, i);
    }
}


void elFileDecoder::WriteMultiWave(elMpegGenerator& gen)
{
    // Get output file name
    std::string filename;
    if (currentPart == 0)
    {
        filename = GenOutputFilename("");
    }
    else
    {
        filename = GenOutputFilename((format("_part%i") % (currentPart + 1)).str());
    }
    
    // Open it and write it
    std::ofstream outFile;
    VERBOSE("Output file: " << filename);
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::binary);
    if (!outFile.is_open())
    {
        throw (runtime_error("Could not open output file '" + filename + "'."));
    }
    
    // Create the streams
    std::vector< shared_ptr<elPcmOutputStream> > Streams;
    unsigned int ChannelCount = 0;
    
    for (unsigned int i = 0; i < gen.GetStreamCount(); i++)
    {
        Streams.push_back(gen.CreatePcmStream(i));
        ChannelCount += gen.GetChannels(i);
    }
    
    if (!Streams.size())
    {
        return;
    }
    
    // Allocate a buffer
    shared_array<short> ReadBuffer(new short[ChannelCount *
    elPcmOutputStream::RecommendBufferSize()]);
    const unsigned int PcmBufferSamples = elPcmOutputStream::RecommendBufferSize();
    shared_array<short> PcmBuffer(new short[PcmBufferSamples]);
    
    // Prepare the wave
    PrepareWaveHeader(outFile);
    
    // Decode
    while (!Streams[0]->Eos())
    {
        unsigned int Ch = 0;
        unsigned int Frames = 0;
        
        for (unsigned int i = 0; i < gen.GetStreamCount(); i++)
        {
            unsigned int Read;
            Read = Streams[i]->Read(PcmBuffer.get(), PcmBufferSamples);
            Frames = Read / Streams[i]->GetChannels();
            
            unsigned int OldCh = Ch;
            
            for (unsigned int j = 0; j < Frames; j++)
            {
                Ch = OldCh;
                
                for (unsigned int k = 0; k < Streams[i]->GetChannels(); k++)
                {
                    ReadBuffer[j * ChannelCount + Ch] = PcmBuffer[j * Streams[i]->GetChannels() + k];
                    Ch++;
                }
            }
        }
        
        outFile.write((char*) ReadBuffer.get(), Frames * ChannelCount * sizeof(short));
    }
    
    const unsigned int SampleCount = ((unsigned int) outFile.tellp() - 44) / 2;
    outFile.seekp(0);
    WriteWaveHeader(outFile, gen.GetSampleRate(0), 16,
                    ChannelCount, SampleCount);
}


void elFileDecoder::WriteMp3OrWave(std::ofstream& output, elMpegGenerator& gen, unsigned int index)
{
    switch (outputFormat)
    {
        case F_MP3:
            WriteMp3(output, gen, index);
            break;
        case F_WAVE:
            WriteWave(output, gen, index);
            break;
    }
}


void elFileDecoder::WriteMp3(std::ofstream& output, elMpegGenerator& gen, unsigned int index)
{
    // Create our buffer
    const unsigned int mpegBufferSize = MAX_MPEG_FRAME_BUFFER;
    shared_array<uint8_t> mpegBuffer(new uint8_t[mpegBufferSize]);
    
    // Now write the stream
    shared_ptr<elMpegOutputStream> stream = gen.CreateMpegStream(index);
    
    while (!stream->Eos())
    {
        unsigned int lastRead;
        lastRead = stream->Read(mpegBuffer.get(), mpegBufferSize);
        output.write((char*) mpegBuffer.get(), lastRead);
    }
}


void elFileDecoder::WriteWave(std::ofstream& output, elMpegGenerator& gen, unsigned int index)
{
    // Create our buffer
    const unsigned int pcmBufferSamples = elPcmOutputStream::RecommendBufferSize();
    shared_array<short> pcmBuffer(new short[pcmBufferSamples]);

    // Create our stream and prepare the wave header
    shared_ptr<elPcmOutputStream> stream = gen.CreatePcmStream(index);
    PrepareWaveHeader(output);
    
    // Write the data
    while (!stream->Eos())
    {
        unsigned int lastRead;
        lastRead = stream->Read(pcmBuffer.get(), pcmBufferSamples);
        output.write((char*) pcmBuffer.get(), lastRead * sizeof(short));
    }
    
    const unsigned int sampleCount = ((unsigned int) output.tellp() - 44) / 2;
    
    // Now write the final wave header
    output.seekp(0);
    WriteWaveHeader(output, gen.GetSampleRate(index), 16,
                    gen.GetChannels(index), sampleCount);
}
