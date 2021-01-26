/*
 *  EA Layer 3 Extractor/Decoder
 *  Copyright (C) 2011, Ben Moench.
 *  See License.txt
 */

#pragma once

#include <string>

class elMpegGenerator;

class elFileDecoder
{
public:
    
    elFileDecoder();
    ~elFileDecoder();
    
    enum Format
    {
        F_AUTO,
        F_MP3,
        F_WAVE,
        F_MULTI_WAVE
    };
    
    enum Parser
    {
        P_AUTO,
        P_VERSION5,
        P_VERSION6
    };
    
    /**
     * Set the input filename and the offset in the input stream to start at.
     */
    void SetInput(const std::string& filename, std::streamoff offset = 0);
    
    /**
     * Get the input filename.
     */
    const std::string& GetInput() const;
    
    /**
     * Set which stream will be decoded. Use -1 to decode all.
     */
    void SetStream(int stream);
    
    int GetStream() const;
    
    void SetParser(Parser parser);
    
    Parser GetParser() const;
    
    /**
     * Set the base output filename as well as the format to try to write to.
     */
    void SetOutput(const std::string& baseFilename, Format format = F_AUTO);
    
    /**
     * Return the output file name.
     */
    const std::string& GetOutputFilename() const;
    
    /**
     * Return the output format.
     */
    Format GetOutputFormat() const;
    
    // TODO add a class to force a certain parser
    
    /**
     * Read the input file and write to the output file(s).
     */
    void Process();
    
    
private:
    std::string inputFilename;
    std::streamoff inputOffset;
    int inputStream;
    Parser inputParser;
    std::string outputFilename;
    Format outputFormat;
    
private:
    int currentPart;
    
    void ProcessPart(std::ifstream& input);
    void AutoSetOutputFormat();
    std::string GenOutputFilename(const std::string& append) const;
    void WriteSingleStream(elMpegGenerator& gen);
    void WriteAllStreams(elMpegGenerator& gen);
    void WriteMultiWave(elMpegGenerator& gen);
    void WriteMp3OrWave(std::ofstream& output, elMpegGenerator& gen, unsigned int index);
    void WriteMp3(std::ofstream& output, elMpegGenerator& gen, unsigned int index);
    void WriteWave(std::ofstream& output, elMpegGenerator& gen, unsigned int index);
};


