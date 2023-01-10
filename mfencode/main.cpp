#include "precomp.h"
#include "mfutil.h"
#include "resource.h"
#include "arguments.h"
using namespace std;
using namespace std::chrono_literals;

void EncodeFile(const std::filesystem::path &input, const std::filesystem::path &output, int quality)
{
    mf::MediaSource source{input.c_str()};
    auto attributes = source.GetAttributes();
    wcout << "Input: " << input.wstring() << endl;
    wcout << "Output: " << output.wstring() << endl;
    wcout << "Duration: " << util::DurationPrinter{attributes.Duration}
          << "; bit depth: " << attributes.BitsPerSample 
          << "; sample rate: " << attributes.SamplesPerSecond
          << "; channels: " << attributes.Channels
          << "; bitrate: " << ((mf::GetAacQualityBytesPerSecond(quality) * 8) / 1000) << "kbps"
          << endl;

    mf::TranscodeSession session{source, output.c_str(), quality};
    session.Start();
    float prevProgress{};
    auto cursorEnable = util::HideCursor();
    auto vtSupport = ookii::vt::virtual_terminal_support::enable_color(ookii::standard_stream::output);
    auto endProgressBar = wil::scope_exit([]() { wcout << endl; });
    util::ShowProgress(0.0f, vtSupport);
    while (!session.Wait(100ms)) 
    {
        auto progress = session.GetProgress();
        if (progress > prevProgress)
        {
            util::ShowProgress(progress, vtSupport);
            prevProgress = progress;
        }
    }

    util::ShowProgress(1.0f, vtSupport);
}

int mfencode_main(Arguments args)
{
    try
    {
        auto com = wil::CoInitializeEx();
        auto mf = mf::Startup();

        std::filesystem::path output{args.Output};
        if (output.empty())
        {
            output = args.Input;
            output.replace_extension(L".m4a");
        }

        if (!args.Force && std::filesystem::exists(output))
        {
            util::WriteError("The output file already exists. Use -Force to overwrite.");
            return 1;
        }

        EncodeFile(args.Input, output, args.Quality);
        return 0;
    }
    catch (const wil::ResultException &ex)
    {
        if (args.Verbose)
        {
            wchar_t msg[2048];
            wil::GetFailureLogString(msg, std::size(msg), ex.GetFailureInfo());
            util::WriteError(msg);
        }
        else
        {
            util::WriteError(util::GetSystemErrorMessage(ex.GetErrorCode()));
        }
    }
    catch (const exception &ex)
    {
        util::WriteError(ex.what());
    }
    catch (...)
    {
        util::WriteError(L"Unknown exception.");
    }

    return 1;
}