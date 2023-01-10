#include "precomp.h"
#include "util.h"
using namespace std;

namespace util
{

namespace details
{

    void SetCursorVisible(bool visible)
    {
        auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info{};
        THROW_LAST_ERROR_IF(!GetConsoleCursorInfo(handle, &info));
        info.bVisible = visible;
        THROW_LAST_ERROR_IF(!SetConsoleCursorInfo(handle, &info));
    }

    void EnableCursor()
    {
        SetCursorVisible(true);
    }
}

[[nodiscard]] unique_cursor_enable HideCursor()
{
    details::SetCursorVisible(false);
    return unique_cursor_enable{};
}


struct WriteColor
{
    WriteColor(bool useColor, const char *color)
        : Color{useColor ? color : nullptr}
    {
    }

    const char *Color;
};

std::wostream &operator<<(std::wostream &stream, WriteColor color)
{
    if (color.Color != nullptr)
    {
        stream << color.Color;
    }

    return stream;
}

void ShowProgress(float progress, bool use_color)
{
    auto width = ookii::get_console_width();
    auto prefix = L"Encoding: "sv;
    // Space for the bar is console width minus: prefix, [], space before percent, percent value, space after percent.
    int barSize = static_cast<int>(width - prefix.length() - 2 - 1 - 4 - 1);
    if (barSize < 0)
    {
        wcout << L'\r' << prefix << std::fixed << std::setprecision(0) << (100 * progress) << L'%' << std::flush;
        return;
    }

    wcout << '\r' << prefix << WriteColor(use_color, ookii::vt::text_format::bright_foreground_blue) << '[';
    int barFilled = static_cast<int>(barSize * progress);
    int i;
    wcout << WriteColor(use_color, ookii::vt::text_format::bright_foreground_green);
    for (i = 0; i < barFilled; ++i)
    {
        wcout << '=';
    }

    for (; i < barSize; ++i)
    {
        wcout << ' ';
    }

    wcout << WriteColor(use_color, ookii::vt::text_format::bright_foreground_blue) << "] " 
           << WriteColor(use_color, ookii::vt::text_format::default_format)
           << fixed << setw(3) << setfill(L' ') << setprecision(0) << (100 * progress) << "%" << std::flush;
}

wstring GetSystemErrorMessage(HRESULT errorCode)
{
    wil::unique_hlocal_string message;
    auto length = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, errorCode, 0,
        reinterpret_cast<LPWSTR>(&message), 0, nullptr);

    if (length == 0)
    {
        return L"Unknown error.";
    }

    return { message.get(), length };
}

}