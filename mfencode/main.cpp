#include "precomp.h"
#include "mfutil.h"
#include "resource.h"
#include <ookii/ookii_main.h>
using namespace std;
using namespace std::chrono_literals;
using namespace ookii::chrono;

namespace details
{
    void EnableCursor()
    {
        ookii::console::SetCursorVisible(true);
    }
}

using unique_cursor_enable = wil::unique_call<decltype(::details::EnableCursor), ::details::EnableCursor>;

[[nodiscard]] auto HideCursor()
{
    ookii::console::SetCursorVisible(false);
    return unique_cursor_enable{};
}

ookii::WinResourceProvider g_resourceProvider{GetModuleHandle(nullptr), true};

struct Arguments
{
    std::filesystem::path Input;
    std::filesystem::path Output;
    int Quality;

    static std::optional<Arguments> Parse(int argc, wchar_t *argv[])
    {
        ookii::CommandLineParser parser{argv[0]};
        try
        {
            Arguments args;
            parser.AddArgument(args.Input, L"Input").Required().Positional().Description(IDS_INPUT_ARGUMENT_DESCRIPTION)
                .AddArgument(args.Output, L"Output").Positional().Description(IDS_OUTPUT_ARGUMENT_DESCRIPTION)
                .AddArgument(args.Quality, L"Quality").Positional().DefaultValue(2).Description(IDS_QUALITY_ARGUMENT_DESCRIPTION);

            parser.Parse(argc, argv);
            if (args.Output.empty())
            {
                args.Output = args.Input;
                args.Output.replace_extension(L".m4a");
            }

            return args;
        }
        catch (const ookii::CommandLineException &ex)
        {
            wcout << ex.Description() << endl;
            parser.WriteUsageToConsole();
            return {};
        }
    }
};

void ShowProgress(float progress)
{
    ookii::console::PrintProgressBar(wcout, progress, L"Encoding: "sv);
}

void EncodeFile(const std::filesystem::path &input, const std::filesystem::path &output, int quality)
{
    mf::MediaSource source{input.c_str()};
    auto attributes = source.GetAttributes();
    wcout << "Input: " << input.wstring() << endl;
    wcout << "Output: " << output.wstring() << endl;
    wcout << "Duration: " << ookii::chrono::DurationPrinter{attributes.Duration, 0}
          << "; bit depth: " << attributes.BitsPerSample 
          << "; sample rate: " << attributes.SamplesPerSecond
          << "; channels: " << attributes.Channels
          << "; bitrate: " << ((mf::GetAacQualityBytesPerSecond(quality) * 8) / 1000) << "kbps"
          << endl;

    mf::TranscodeSession session{source, output.c_str(), quality};
    session.Start();
    float prevProgress{};
    unique_cursor_enable reenable = HideCursor();
    ShowProgress(0.0f);
    while (!session.Wait(100ms)) 
    {
        auto progress = session.GetProgress();
        if (progress > prevProgress)
        {
            ShowProgress(progress);
            prevProgress = progress;
        }
    }

    ShowProgress(1.0f);
    wcout << endl;
}

/// res:IDS_APP_DESCRIPTION
// input (Path): res:IDS_INPUT_ARGUMENT_DESCRIPTION
// output (Path): res:IDS_OUTPUT_ARGUMENT_DESCRIPTION
// quality: res:IDS_QUALITY_ARGUMENT_DESCRIPTION
int ookii_main(std::filesystem::path input, std::filesystem::path output = {}, int quality = 2)
{
    try
    {
        ookii::console::EnableVirtualTerminalSequences();
        auto com = wil::CoInitializeEx();
        auto mf = mf::Startup();

        if (output.empty())
        {
            output = input;
            output.replace_extension(L".m4a");
        }

        EncodeFile(input, output, quality);

        return 0;
    }
    catch (const ookii::Exception &ex)
    {
        wcout << ex.Description() << endl;
    }
    catch (const exception &ex)
    {
        cout << ex.what() << endl;
    }
    catch (...)
    {
        cout << "Unknown exception." << endl;
    }

    return 1;
}

#include <main_generated.cpp>
