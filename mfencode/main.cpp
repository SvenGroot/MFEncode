#include "precomp.h"
#include "mfutil.h"
#include "resource.h"
#include "arguments.h"
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

int mfencode_main(Arguments args)
{
    try
    {
        ookii::console::EnableVirtualTerminalSequences();
        auto com = wil::CoInitializeEx();
        auto mf = mf::Startup();

        std::filesystem::path output{args.Output};
        if (output.empty())
        {
            output = args.Input;
            output.replace_extension(L".m4a");
        }

        EncodeFile(args.Input, output, args.Quality);

        return 0;
    }
    catch (const ookii::Exception &ex)
    {
        wcerr << ex.Description() << endl;
    }
    catch (const wil::ResultException &ex)
    {
        wcerr << ookii::GetSystemErrorMessage(ex.GetErrorCode()) << endl;
    }
    catch (const exception &ex)
    {
        cerr << ex.what() << endl;
    }
    catch (...)
    {
        wcerr << L"Unknown exception." << endl;
    }

    return 1;
}