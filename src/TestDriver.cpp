/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"

#include <fstream>
#include <boost/format.hpp>

#include "Version.h"
#include "AllFormats.h"
#include "MpegGenerator.h"
#include "MpegOutputStream.h"
#include "PcmOutputStream.h"

int g_Verbose = 1;

int main(int Argc, char **Argv)
{
    // Show a small banner.
    std::cout << "Version ";
    std::cout << ealayer3_VERSION_MAJOR << "." << ealayer3_VERSION_MINOR << "." << ealayer3_VERSION_PATCH;
    std::cout << std::endl;
    
    // Check the arguments.
    if (Argc != 2)
    {
        std::cout << "Invalid argument(s). Call with input file name as the only argument." << std::endl;
        std::cout << std::endl;
        return 1;
    }
    const std::string InputFilename = Argv[1];

    // Open the input file.
    std::ifstream Input;
    Input.open(InputFilename.c_str(), std::ios_base::in | std::ios_base::binary);
    if (!Input.is_open())
    {
        std::cout << "Could not open input file '" << InputFilename << "'." << std::endl;
        return 1;
    }

    elBlockLoaderSelector Loader;
    elMpegGenerator Gen;
    try
    {
        // Determine the input's file type.
        if (!Loader.Initialize(&Input))
        {
            std::cout << "The input is not in a readable file format." << std::endl;
            return 1;
        }

        // Grab the first block and initialize the generator.
        elBlock FirstBlock;
        if (!Loader.ReadNextBlock(FirstBlock))
        {
            std::cout << "The first block could not be read from the input." << std::endl;
            return 1;
        }

        if (!Gen.Initialize(FirstBlock, Loader.CreateParser()))
        {
            std::cout << "The EALayer3 parser could not be initialized (the bitstream format is not readable)." << std::endl;
            return 1;
        }

        // Parse the first block.
        Gen.ParseBlock(FirstBlock);
    }
    catch (elParserException& E)
    {
        std::cout << "While reading the first block: " << std::endl;
        std::cout << "    Problems reading the input file." << std::endl;
        std::cout << "    Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (std::exception& E)
    {
        std::cout << "While reading the first block: " << std::endl;
        std::cout << "    There was an error." << std::endl;
        std::cout << "    Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout << "While reading the first block: " << std::endl;
        std::cout << "    Unhandled exception." << std::endl;
        return 1;
    }

    // Show some info.
    std::cout << "Stream count: " << Gen.GetStreamCount() << std::endl;

    // Parse the rest of the blocks.
    try
    {
        while (true)
        {
            elBlock Block;
            if (!Loader.ReadNextBlock(Block))
            {
                break;
            }
            Gen.ParseBlock(Block);
        }
        Gen.DoneParsingBlocks();
    }
    catch (elParserException& E)
    {
        std::cout << "While parsing the blocks: " << std::endl;
        std::cout << "    Problems reading the input file." << std::endl;
        std::cout << "    Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (std::exception& E)
    {
        std::cout << "While parsing the blocks: " << std::endl;
        std::cout << "    There was an error." << std::endl;
        std::cout << "    Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout << "While parsing the blocks: " << std::endl;
        std::cout << "    Unhandled exception." << std::endl;
        return 1;
    }

    // Grab some output.
    try
    {
        const unsigned int PcmBufferSamples = elPcmOutputStream::RecommendBufferSize();
        shared_array<short> PcmBuffer(new short[PcmBufferSamples]);
    
        for (unsigned int i = 0; i < Gen.GetStreamCount(); i++)
        {
            shared_ptr<elPcmOutputStream> Stream;
            Stream = Gen.CreatePcmStream(i);

            while (!Stream->Eos())
            {
                Stream->Read(PcmBuffer.get(), PcmBufferSamples);
            }
        }
    }
    catch (std::exception& E)
    {
        std::cout << "While decoding: " << std::endl;
        std::cout << "    There was an error." << std::endl;
        std::cout << "    Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout << "While decoding: " << std::endl;
        std::cout << "    Unhandled exception." << std::endl;
        return 1;
    }

    // Show some info.
    Input.clear();
    std::cout << "Uncompressed sample frames: " << Gen.GetUncSampleFrameCount() << std::endl;
    std::cout << "End offset in file: " << Input.tellg() << std::endl;
    return 0;
}
