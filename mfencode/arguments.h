#pragma once

#include <filesystem>

// [arguments]
// Encodes audio files to AAC format using the Media Foundation codec.
struct Arguments
{
    // [argument, required, positional]
    // [value_description: path]
    // The path of the input media file.
    std::wstring Input;
    // [argument, positional]
    // [value_description: path]
    // The path of the output AAC file. If not specified, it will be the input path with the
    // extension replaced by '.m4a'.
    std::wstring Output;
    // [argument, positional, default: 2]
    // The quality of the output file. Possible values: 
    // 
    // 1: 96kbps; 2: 128kbps; 3: 160kbps; 4: 192kbps;
    int Quality;

    OOKII_DECLARE_PARSE_METHOD(Arguments);
};

int mfencode_main(Arguments args);