/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010-2011, Ben Moench.
    See License.txt
*/

#include "Internal.h"

#include <fstream>
#include <boost/format.hpp>

#include "FileDecoder.h"

#include "Version.h"
#include "AllFormats.h"
#include "MpegGenerator.h"
#include "MpegOutputStream.h"
#include "PcmOutputStream.h"
#include "WaveWriter.h"
#include "Parsers/ParserVersion6.h"

#include "MpegParser.h"
#include "Generator.h"
#include "Writers/HeaderlessWriter.h"
#include "Writers/SingleBlockWriter.h"
#include "Writers/HeaderBWriter.h"

#include "Bitstream.h"

int g_Verbose = 0;

enum EOutputFormat
{
    EOF_AUTO,
    EOF_MP3,
    EOF_WAVE,
    EOF_MULTI_WAVE,
    EOF_EALAYER3
};

enum EParser
{
    EP_AUTO,
    EP_VERSION5,
    EP_VERSION6
};

enum EOutputEALayer3
{
    EOEA_HEADERLESS,
    EOEA_SINGLEBLOCK,
    EOEA_HEADERB,
    EOEA_TWOFILES
};

struct SArguments
{
    SArguments() :
        ShowBanner(true),
        ShowUsage(false),
        ShowInfo(false),
        Parser(EP_AUTO),
        InputFilename(""),
        OutputFilename(""),
        StreamIndex(0),
        AllStreams(false),
        Offset(0),
        OutputFormat(EOF_AUTO),
        OutputEALayer3(EOEA_HEADERLESS),
        OutputLoop(false),

        DecodeParser(elFileDecoder::P_AUTO),
        DecodeOutFormat(elFileDecoder::F_AUTO)
    {
    };

    bool ShowBanner;
    bool ShowUsage;
    bool ShowInfo;
    EParser Parser;
    std::string InputFilename;
    std::string OutputFilename;
    unsigned int StreamIndex;
    bool AllStreams;
    std::streamoff Offset;
    EOutputFormat OutputFormat;
    EOutputEALayer3 OutputEALayer3;
    bool OutputLoop;

    elFileDecoder::Parser DecodeParser;
    elFileDecoder::Format DecodeOutFormat;

    std::vector<std::string> InputFilenameVector;
};

// Functions in this file
void SeparateFilename(const std::string& Filename, std::string& PathAndName, std::string& Ext);
bool ParseArguments(SArguments& Args, unsigned long Argc, char* Argv[]);
void ShowUsage(const std::string& Program);
bool OpenOutputFile(std::ofstream& Output, const std::string& Filename);
int Encode(SArguments& Args);


void SeparateFilename(const std::string& Filename, std::string& PathAndName, std::string& Ext)
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

bool ParseArguments(SArguments& Args, unsigned long Argc, char* Argv[])
{
    for (unsigned int i = 1; i < Argc;)
    {
        std::string Arg(Argv[i++]);

        // Check it

        if (Arg == "-b-" || Arg == "--no-banner")
        {
            Args.ShowBanner = false;
        }
        else if (Arg == "-o" || Arg == "--output")
        {
            if (i >= Argc)
            {
                return false;
            }

            Args.OutputFilename = std::string(Argv[i++]);
        }
        else if (Arg == "-s" || Arg == "--stream")
        {
            if (i >= Argc)
            {
                return false;
            }

            if (std::string(Argv[i]) == "all")
            {
                Args.AllStreams = true;
                Args.StreamIndex = 0;
                i++;
            }
            else
            {
                Args.StreamIndex = atoi(Argv[i++]) - 1;
                Args.AllStreams = false;
            }
        }
        else if (Arg == "-i" || Arg == "--offset")
        {
            if (i >= Argc)
            {
                return false;
            }

            Args.Offset = atoi(Argv[i++]);
        }
        else if (Arg == "-n" || Arg == "--info")
        {
            Args.ShowInfo = true;
        }
        else if (Arg == "-w" || Arg == "--wave")
        {
            Args.OutputFormat = EOF_WAVE;
            Args.DecodeOutFormat = elFileDecoder::F_WAVE;
        }
        else if (Arg == "-mc" || Arg == "--multi-wave")
        {
            Args.OutputFormat = EOF_MULTI_WAVE;
            Args.DecodeOutFormat = elFileDecoder::F_MULTI_WAVE;
        }
        else if (Arg == "-m" || Arg == "--mp3")
        {
            Args.OutputFormat = EOF_MP3;
            Args.DecodeOutFormat = elFileDecoder::F_MP3;
        }
        else if (Arg == "-v" || Arg == "--verbose")
        {
            g_Verbose = 1;
        }
        else if (Arg == "--parser5")
        {
            Args.Parser = EP_VERSION5;
            Args.DecodeParser = elFileDecoder::P_VERSION5;
        }
        else if (Arg == "--parser6")
        {
            Args.Parser = EP_VERSION6;
            Args.DecodeParser = elFileDecoder::P_VERSION6;
        }
        else if (Arg == "--single-block")
        {
            Args.OutputEALayer3 = EOEA_SINGLEBLOCK;
        }
        else if (Arg == "--header-b")
        {
            Args.OutputEALayer3 = EOEA_HEADERB;
        }
        else if (Arg == "--two-files")
        {
            Args.OutputEALayer3 = EOEA_TWOFILES;
        }
        else if (Arg == "--loop")
        {
            Args.OutputLoop = true;
        }
        else if (Arg == "-E")
        {
            Args.OutputFormat = EOF_EALAYER3;
        }
        else
        {
            Args.InputFilename = Arg;
            Args.InputFilenameVector.push_back(Arg);
        }
    }

    return true;
}

void ShowUsage(const std::string& Program)
{
    std::cout << "Usage: " << Program << " InputFilename [Options]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -i, --offset Offset   Specify the offset in the file to begin at." << std::endl;
    std::cout << "  -o, --output File     Specify the output filename (.mp3)." << std::endl;
    std::cout << "  -s, --stream Index    Specify which stream to extract, or all." << std::endl;
    std::cout << "  -m, --mp3             Output to MP3 (no information loss!)." << std::endl;
    std::cout << "  -w, --wave            Output to Microsoft WAV." << std::endl;
    std::cout << "  -mc, --multi-wave     Output to a multi-channel Microsoft WAV." << std::endl;
    std::cout << "  --parser5             Force using the version 5 parser." << std::endl;
    std::cout << "  --parser6             Force using the version 6/7 parser." << std::endl;
    std::cout << "  -n, --info            Output information about the file." << std::endl;
    std::cout << "  -v, --verbose         Be verbose (useful when streams won't convert)." << std::endl;
    std::cout << "  -b-, --no-banner      Don't show the banner." << std::endl;
    std::cout << std::endl;
    std::cout << "Encoding: " << Program << " -E InputFile [InputFile2 ...] [Options]" << std::endl;
    std::cout << "  --single-block        Create a stream in the single-block format. " << std::endl;
    std::cout << "  --header-b            Create a stream in the header B format. " << std::endl;
    std::cout << "  --two-files           Create a stream in the headerless format and a header in the single-block format. " << std::endl;
    std::cout << "  --loop                Mark the file as loop (for single-block or two-files). " << std::endl;
    std::cout << std::endl;
    std::cout << "If multiple input files are given, they will be be interleaved ";
    std::cout << "into multiple streams" << std::endl << std::endl;
    std::cout << "Supported formats: " << std::endl;

    // List supported formats
    elBlockLoaderSelector Loader;

    for (elBlockLoaderSelector::fsFormatList::const_iterator Fmt = Loader.SelectorList().begin();
        Fmt != Loader.SelectorList().end(); ++Fmt)
    {
        std::cout << "   * " << (*Fmt)->GetName() << std::endl;

        // Get supported parsers
        std::vector<std::string> Parsers;
        (*Fmt)->ListSupportedParsers(Parsers);

        for (std::vector<std::string>::const_iterator Parser = Parsers.begin();
            Parser != Parsers.end(); ++Parser)
        {
            std::cout << "      - " << (*Parser) << std::endl;
        }
    }

    std::cout << std::endl;

    return;
}

int main(int Argc, char** Argv)
{
    // Parse the arguments
    SArguments Args;
    bool ArgParse;

    if (Argc == 1)
    {
        Args.ShowUsage = true;
    }

    ArgParse = ParseArguments(Args, Argc, Argv);

    // Display banner
    if (Args.ShowBanner)
    {
        std::cerr << "EA Layer 3 Stream Extractor/Decoder ";
        std::cerr << ealayer3_VERSION_MAJOR << "." << ealayer3_VERSION_MINOR << "." << ealayer3_VERSION_PATCH;
        std::cerr << ". Copyright (C) 2010-11, Ben Moench." << std::endl;
        std::cerr << std::endl;
    }

    // Display usage
    if (Args.ShowUsage)
    {
        ShowUsage(Argv[0]);
    }

    // Check for errors
    if (!ArgParse)
    {
        std::cerr << "The arguments are not valid." << std::endl;
        return 1;
    }

    if (Args.InputFilename.empty())
    {
        std::cerr << "You must specify an input filename." << std::endl;
        return 1;
    }

    // Do we want to encode the file?
    if ((Args.OutputFormat == EOF_EALAYER3))
    {
        try
        {
            return Encode(Args);
        }
        catch (elParserException& E)
        {
            std::cerr << "Problems reading the input file." << std::endl;
            std::cerr << "Exception: " << E.what() << std::endl;
            return 1;
        }
        catch (std::exception& E)
        {
            std::cerr << "There was an error." << std::endl;
            std::cerr << "Exception: " << E.what() << std::endl;
            return 1;
        }
        catch (...)
        {
            std::cerr << "Crashed." << std::endl;
            return 1;
        }
    }

    // Decode the file
    try
    {
        elFileDecoder decoder;

        decoder.SetInput(Args.InputFilename, Args.Offset);
        decoder.SetParser(Args.DecodeParser);

        if (Args.AllStreams)
        {
            decoder.SetStream(-1);
        }
        else
        {
            decoder.SetStream(Args.StreamIndex);
        }

        decoder.SetOutput(Args.OutputFilename, Args.DecodeOutFormat);
        decoder.Process();
    }
    catch (elParserException& E)
    {
        std::cerr << "Problems reading the input file." << std::endl;
        std::cerr << "Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (std::runtime_error& E)
    {
        std::cerr << E.what() << std::endl;
        return 1;
    }
    catch (std::exception& E)
    {
        std::cerr << "There was an error." << std::endl;
        std::cerr << "Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Crashed." << std::endl;
        return 1;
    }

    return 0;
}



void CreateOutputFilename(SArguments& Args)
{
    std::string PathAndName;
    std::string Ext;

    SeparateFilename(Args.InputFilenameVector[0], PathAndName, Ext);
    Args.OutputFilename = PathAndName;

    switch (Args.OutputFormat)
    {

    case EOF_MP3:
        Args.OutputFilename.append(".mp3");
        break;

    case EOF_WAVE:
        Args.OutputFilename.append(".wav");
        break;

    case EOF_MULTI_WAVE:
        Args.OutputFilename.append(".wav");
        break;

    case EOF_EALAYER3:
        Args.OutputFilename.append(".ealayer3");
        break;
    }

    return;
}

bool OpenOutputFile(std::ofstream& Output, const std::string& Filename)
{
    Output.open(Filename.c_str(), std::ios_base::out | std::ios_base::binary);

    if (!Output.is_open())
    {
        std::cerr << "Could not open output file '" << Filename << "'." << std::endl;
        return false;
    }

    return true;
}

struct elEncodeInput
{
    elEncodeInput(const std::string& MpegFile, const std::string& WaveFile) :
        MpegFile(MpegFile), WaveFile(WaveFile) {}

    inline void SetupFrames()
    {
        CurrentFrame = &Frame1;
        LastFrame = &Frame2;
        LastFrame->Gr[0].Used = false;
    }

    std::string MpegFile;
    std::string WaveFile;

    shared_ptr<std::ifstream> MpegInput;
    shared_ptr<std::ifstream> WaveInput;

    shared_ptr<elMpegParser> MpegParser;

    // MpegParser? WaveParser?

    elFrame Frame1;
    elFrame Frame2;
    elFrame* CurrentFrame;
    elFrame* LastFrame;
};

typedef std::vector<elEncodeInput> elEncodeInputVector;

int Encode(SArguments& Args)
{
    // Create an output filename if there isn't already one
    bool ShowOutputFile = false;

    if (Args.OutputFilename.empty() && !Args.ShowInfo)
    {
        CreateOutputFilename(Args);
        ShowOutputFile = true;
    }

    // The container for the inputs for each stream
    elEncodeInputVector InputFiles;

    // Parse the input parameters
    for (std::vector<std::string>::const_iterator Iter = Args.InputFilenameVector.begin();
        Iter != Args.InputFilenameVector.end(); ++Iter)
    {
        // Add this to the inputs
        InputFiles.push_back(elEncodeInput(*Iter, ""));

        // Create an MpegParser and perhaps a WaveParser for each of the inputs
        shared_ptr<std::ifstream> Input = make_shared<std::ifstream>();
        Input->open(InputFiles.back().MpegFile.c_str(), std::ios_base::in | std::ios_base::binary);
        if (!Input->is_open())
        {
            std::cerr << "Could not open input file '" << InputFiles.back().MpegFile.c_str() << "'." << std::endl;
            return 1;
        }
        InputFiles.back().MpegInput = Input;

        // Create the parser
        shared_ptr<elMpegParser> Parser = make_shared<elMpegParser>();
        Parser->Initialize(Input.get());
        InputFiles.back().MpegParser = Parser;
    }

    // This is needed because otherwise the pointers to frames will be invalidated
    // when a new item is added to the vector.
    for (elEncodeInputVector::iterator Iter = InputFiles.begin();
        Iter != InputFiles.end(); ++Iter)
    {
        Iter->SetupFrames();
    }

    // Open output file
    std::ofstream Output;
    if (!OpenOutputFile(Output, Args.OutputFilename))
    {
        return 1;
    }

    // The generator and writer
    elGenerator Gen;
    shared_ptr<elBlockWriter> Writer;

    switch (Args.OutputEALayer3)
    {
    case EOEA_SINGLEBLOCK:
        Writer = make_shared<elSingleBlockWriter>(Args.OutputLoop);
        break;
    case EOEA_HEADERB:
        Writer = make_shared<elHeaderBWriter>();
        break;
    default:
        Writer = make_shared<elHeaderlessWriter>();
        break;
    }
    Writer->Initialize(&Output);

    // Some variables
    elBlock Block;
    std::vector<elBlock> AllBlocks;
    bool NoMoreFrames = false;
    bool First = true;
    bool WasUsed = false;
    unsigned int SampleRate = 0;
    unsigned int Channels = 0;

    // The loop that parses all the MPEG frames and generates EALayer3 blocks
    while (!NoMoreFrames)
    {
        // Read the next frames in if we're not done
        for (elEncodeInputVector::iterator Iter = InputFiles.begin();
            Iter != InputFiles.end(); ++Iter)
        {
            elMpegParser& Parser = *Iter->MpegParser;
            elFrame& CurrentFrame = *Iter->CurrentFrame;

            int countCheck = 0;
            do
            {
                if (!Parser.ReadFrame(CurrentFrame) || countCheck > 15)
                {
                    NoMoreFrames = true;
                    break;
                }
                countCheck++;
            } while (!CurrentFrame.Gr[0].Used);
        }

        // Get the number of channels
        unsigned int NewChannels = 0;
        for (elEncodeInputVector::iterator Iter = InputFiles.begin();
            Iter != InputFiles.end(); ++Iter)
        {
            elFrame& CurrentFrame = *Iter->CurrentFrame;

            if (CurrentFrame.Gr[0].Used)
            {
                NewChannels += CurrentFrame.Gr[0].Channels;
                SampleRate = CurrentFrame.Gr[0].SampleRate;
            }
        }
        if (Channels == 0)
        {
            Channels = NewChannels;
        }

        // Add the last frame if it was used for each of the streams
        for (elEncodeInputVector::iterator Iter = InputFiles.begin();
            Iter != InputFiles.end(); ++Iter)
        {
            elFrame*& LastFrame = Iter->LastFrame;
            elFrame*& CurrentFrame = Iter->CurrentFrame;

            if (LastFrame->Gr[0].Used)
            {
                if (First)
                {
                    elUncompressedSampleFrames& Usf = LastFrame->Gr[1].Uncomp; // need to be in second granule
                    unsigned int TotalCount;
                    WasUsed = true;
                    Usf.Count = ENCODER_UNCOM_SAMPLES;
                    TotalCount = Usf.Count * LastFrame->Gr[0].Channels;
                    Usf.Data = shared_array<short>(new short[TotalCount]);
                    memset(Usf.Data.get(), 0, TotalCount * sizeof(short));

                    //std::cout << ": " << (void*)LastFrame->Gr[0].Channels << std::endl;
                }

                Gen.AddFrameFromStream(*LastFrame);
            }

            // Now the current and last frames are swapped
            elFrame* Temp = LastFrame;
            LastFrame = CurrentFrame;
            CurrentFrame = Temp;
        }

        // Write the block
        if (Gen.Generate(Block, First && WasUsed))
        {
            if (Args.OutputEALayer3 != EOEA_SINGLEBLOCK) {
                Writer->WriteNextBlock(Block, NoMoreFrames);
            }
            if (Args.OutputEALayer3 == EOEA_SINGLEBLOCK || Args.OutputEALayer3 == EOEA_TWOFILES) {
                AllBlocks.push_back(Block);
                Block.Data.reset();
                Block.Clear();
            }
        }

        if (First && WasUsed) // etait avant write
        {
            First = false;
        }
    }

    if (Args.OutputEALayer3 == EOEA_SINGLEBLOCK || Args.OutputEALayer3 == EOEA_TWOFILES)
    {
        elBlock ActualBlock;
        ActualBlock.Size = 0;
        ActualBlock.SampleCount = 0;

        for (std::vector<elBlock>::const_iterator Iter = AllBlocks.begin();
            Iter != AllBlocks.end(); ++Iter)
        {
            ActualBlock.Size += Iter->Size;
            ActualBlock.SampleCount += Iter->SampleCount;
        }

        ActualBlock.Channels = Channels;
        ActualBlock.SampleRate = SampleRate;

        if (Args.OutputEALayer3 == EOEA_SINGLEBLOCK) {
            ActualBlock.Data = shared_array<uint8_t>(new uint8_t[ActualBlock.Size]);

            uint8_t* Ptr = ActualBlock.Data.get();
            for (std::vector<elBlock>::const_iterator Iter = AllBlocks.begin();
                Iter != AllBlocks.end(); ++Iter)
            {
                memcpy(Ptr, Iter->Data.get(), Iter->Size);
                Ptr += Iter->Size;
            }

            Ptr = NULL;
            AllBlocks.clear();

            Writer->WriteNextBlock(ActualBlock, true);
        }
        else {
            std::ofstream OutputH;
            if (!OpenOutputFile(OutputH, Args.OutputFilename + ".header"))
            {
                return 1;
            }
            shared_ptr<elSingleBlockWriter> WriterH = make_shared<elSingleBlockWriter>(Args.OutputLoop);
            WriterH->Initialize(&OutputH);
            WriterH->WriteHeader(ActualBlock);
        }
    }
    return 0;
}