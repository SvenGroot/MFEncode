#include "precomp.h"
#include "mfutil.h"
using namespace std;
using namespace std::chrono_literals;
using namespace ookii::chrono;

using unique_cursor_enable = wil::unique_call<decltype(ookii::EnableConsoleCursor), ookii::EnableConsoleCursor>;

std::wstring ChangeExtension(std::wstring_view path, std::wstring_view newExtension)
{
    auto index = path.find_last_of(L"./\\");
    std::wstring_view base = path;
    if (index != std::wstring_view::npos && path[index] == L'.')
    {
        base = path.substr(0, index);
    }

    std::wstring result{base};
    result.append(newExtension);
    return result;
}

struct Arguments
{
    std::wstring Input;
    std::wstring Output;

    static std::optional<Arguments> Parse(int argc, wchar_t *argv[])
    {
        ookii::CommandLineParser parser{argv[0]};
        try
        {
            Arguments args;
            parser.AddArgument(args.Input, L"Input").Required().Positional().ValueDescription(L"Path")
                .AddArgument(args.Output, L"Output").Positional().ValueDescription(L"Path");

            parser.Parse(argc, argv);
            if (args.Output.length() == 0)
            {
                args.Output = ChangeExtension(args.Input, L".m4a"sv);
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
    ookii::PrintProgressBar(wcout, progress, L"Encoding: "sv);
}

void EncodeFile(const Arguments &args)
{
    mf::MediaSource source{args.Input.c_str()};
    auto attributes = source.GetAttributes();
    wcout << "Input: " << args.Input << endl;
    wcout << "Output: " << args.Output << endl;
    wcout << "Duration: " << ookii::chrono::DurationPrinter{attributes.Duration, 0}
          << "; bit depth: " << attributes.BitsPerSample 
          << "; sample rate: " << attributes.SamplesPerSecond
          << "; channels: " << attributes.Channels
          << endl;

    mf::TranscodeSession session{source, args.Output.c_str(), 24000};
    session.Start();
    float prevProgress{};
    ookii::DisableConsoleCursor();
    unique_cursor_enable reenable;
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

int wmain(int argc, wchar_t *argv[])
{
    try
    {
        auto args = Arguments::Parse(argc, argv);
        if (!args)
        {
            return 1;
        }

        auto com = wil::CoInitializeEx();
        auto mf = mf::Startup();
        EncodeFile(*args);

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