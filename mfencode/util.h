#pragma once

namespace util
{

namespace details
{
    void EnableCursor();

    struct WriteColor
    {
        WriteColor(bool useColor, const char *color)
            : Color{useColor ? color : nullptr}
        {
        }

        const char *Color;
    };
}

using unique_cursor_enable = wil::unique_call<decltype(details::EnableCursor), details::EnableCursor>;

[[nodiscard]] unique_cursor_enable HideCursor();

void ShowProgress(float progress, bool use_color);

std::wstring GetSystemErrorMessage(HRESULT errorCode);

// Windows 100ns units:
using WindowsTimeUnits = std::chrono::duration<long long, std::ratio<1, wil::filetime_duration::one_second>>;
using Days = std::chrono::duration<int, std::ratio<24 * 3600>>;
using HoursPerDay = std::ratio_divide<Days::period, std::chrono::hours::period>;
using MinutesPerHour = std::ratio_divide<std::chrono::hours::period, std::chrono::minutes::period>;;
using SecondsPerMinute = std::ratio_divide<std::chrono::minutes::period, std::chrono::seconds::period>;;

template<typename Rep, typename Period>
constexpr std::chrono::duration<double> TotalSeconds(std::chrono::duration<Rep, Period> duration)
{
    return std::chrono::duration_cast<std::chrono::duration<double>>(duration);
}

template<typename Rep, typename Period>
constexpr Days DaysComponent(std::chrono::duration<Rep, Period> duration)
{
    return std::chrono::duration_cast<Days>(duration);
}

template<typename Rep, typename Period>
constexpr std::chrono::hours HoursComponent(std::chrono::duration<Rep, Period> duration)
{
    static_assert(HoursPerDay::den == 1);
    return std::chrono::duration_cast<std::chrono::hours>(duration) % HoursPerDay::num;
}

template<typename Rep, typename Period>
constexpr std::chrono::minutes MinutesComponent(std::chrono::duration<Rep, Period> duration)
{
    static_assert(MinutesPerHour::den == 1);
    return std::chrono::duration_cast<std::chrono::minutes>(duration) % MinutesPerHour::num;
}

template<typename Rep, typename Period>
constexpr std::chrono::seconds SecondsComponent(std::chrono::duration<Rep, Period> duration)
{
    static_assert(SecondsPerMinute::den == 1);
    return std::chrono::duration_cast<std::chrono::seconds>(duration) % SecondsPerMinute::num;
}

template<typename Rep, typename Period>
constexpr std::chrono::milliseconds MillisecondsComponent(std::chrono::duration<Rep, Period> duration)
{
    static_assert(std::milli::num == 1);
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration) % std::milli::den;
}

template<typename Rep, typename Period>
constexpr std::chrono::microseconds MicrosecondsComponent(std::chrono::duration<Rep, Period> duration)
{
    static_assert(std::micro::num == 1);
    return std::chrono::duration_cast<std::chrono::microseconds>(duration) % std::micro::den;
}

namespace details
{

    inline std::ostream &setzerofill(std::ostream &stream)
    {
        return stream << std::setfill('0');
    }

    inline std::wostream &setzerofill(std::wostream &stream)
    {
        return stream << std::setfill(L'0');
    }

}

template<typename Rep, typename Period>
class DurationPrinter
{
public:
    using DurationType = std::chrono::duration<Rep, Period>;

    DurationPrinter(DurationType duration, int subSecondPrecision = 6)
        : _duration{duration},
          _subSecondPrecision{std::min(subSecondPrecision, 6)}
    {
    }

    std::wostream& operator()(std::wostream &stream) const
    {
        auto duration = std::chrono::abs(_duration);
        if (_duration.count() < 0)
        {
            stream << '-';
        }

        auto days = DaysComponent(duration).count();
        if (days > 0)
        {
            stream << days << '.';
        }

        auto hours = HoursComponent(duration).count();
        if (hours > 0 || days > 0)
        {
            stream << std::setw(2) << std::setfill(L'0') << hours << ':';
        }

        stream << std::setw(2) << std::setfill(L'0') << MinutesComponent(duration).count() << ':';
        stream << std::setw(2) << std::setfill(L'0') << SecondsComponent(duration).count();
        if (_subSecondPrecision > 0) 
        {
            auto component = MicrosecondsComponent(duration).count();
            if (_subSecondPrecision < 6)
            {
                double partialComponent = static_cast<double>(component);
                for (int x = 6; x > _subSecondPrecision; --x)
                {
                    partialComponent /= 10;
                }

                component = static_cast<Rep>(std::round(partialComponent));
            }

            stream << std::use_facet<std::numpunct<wchar_t>>(stream.getloc()).decimal_point();
            stream << std::setw(_subSecondPrecision) << details::setzerofill << component;
        }

        return stream;
    }

private:
    DurationType _duration;
    int _subSecondPrecision;
};

template<typename Rep, typename Period>
std::wostream &operator<<(std::wostream &stream, const util::DurationPrinter<Rep, Period> &printer)
{
    return printer(stream);
}

template<typename T>
void WriteError(const T &error)
{
    auto support = ookii::vt::virtual_terminal_support::enable_color(ookii::standard_stream::error);
    auto stream = ookii::wline_wrapping_ostream::for_cerr();
    if (support)
    {
        stream << ookii::vt::text_format::foreground_red;
    }

    stream << "An error has occurred: " << error << std::endl;
    if (support)
    {
        stream << ookii::vt::text_format::default_format;
    }
}

}