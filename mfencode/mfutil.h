#pragma once

#include "util.h"

namespace mf
{

using unique_mfshutdown_call = wil::unique_call<decltype(::MFShutdown), ::MFShutdown>;

[[nodiscard]] unique_mfshutdown_call Startup();

struct MediaAttributes
{
    util::WindowsTimeUnits Duration;
    UINT32 BitsPerSample;
    UINT32 SamplesPerSecond;
    UINT32 Channels;
};

class MediaSource
{
public:
    MediaSource(PCWSTR input);
    
    MediaAttributes GetAttributes() const;
    IMFMediaSource *Get();

private:
    wil::com_ptr<IMFMediaSource> m_source;
};

class ISessionEvents
{
public:
    virtual void OnSessionEnded() = 0;
    virtual void OnSessionClosed() = 0;
    virtual void OnError(std::exception_ptr exception) = 0;
    virtual IMFMediaSession *GetMediaSession() = 0;
    virtual IMFTopology *GetTopology() = 0;
};

class SessionEventSink
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
                                          IMFAsyncCallback>
{
public:
    SessionEventSink(ISessionEvents &eventHandler);
    
    STDMETHODIMP GetParameters(DWORD *flags, DWORD *queue) override;
    STDMETHODIMP Invoke(IMFAsyncResult *result) override;

    void BeginGetEvent();

    static wil::com_ptr<SessionEventSink> Create(ISessionEvents &eventHandler);

private:

    ISessionEvents &m_eventHandler;
};

class TranscodeSession final : private ISessionEvents
{
public:
    TranscodeSession(MediaSource &source, PCWSTR output, int quality);

    void Start();
    bool Wait(std::chrono::milliseconds timeout);
    util::WindowsTimeUnits GetPosition() const;
    float GetProgress() const;

private:
    void OnSessionEnded() override;
    void OnSessionClosed() override;
    void OnError(std::exception_ptr exception) override;
    IMFMediaSession *GetMediaSession() override;
    IMFTopology *GetTopology() override;

    wil::com_ptr<IMFMediaSession> m_session;
    wil::com_ptr<IMFTopology> m_topology;
    wil::com_ptr<SessionEventSink> m_eventSink;
    wil::com_ptr<IMFPresentationClock> m_clock;
    wil::unique_event m_waitEvent;
    std::exception_ptr m_exception;
    util::WindowsTimeUnits m_duration{};
};

class AttributeHelper
{
public:
    AttributeHelper(IMFAttributes *attributes);
    AttributeHelper(wil::com_ptr<IMFAttributes> &attributes);

    UINT32 GetUINT32(const GUID &key);
    UINT64 GetUINT64(const GUID &key);

    void Set(const GUID &key, const GUID &value);
    void Set(const GUID &key, UINT32 value);

private:
    IMFAttributes *m_attributes;
};

wil::com_ptr<IMFSourceResolver> CreateSourceResolver();
wil::com_ptr<IMFTranscodeProfile> CreateTranscodeProfile();
wil::com_ptr<IMFAttributes> CreateAttributes(UINT32 initialSize);
wil::com_ptr<IMFTopology> CreateTopology(IMFMediaSource *source, PCWSTR output, IMFTranscodeProfile *profile);
wil::com_ptr<IMFMediaSession> CreateMediaSession(IMFAttributes *configuration = nullptr);
wil::com_ptr<IMFTranscodeProfile> CreateAacTranscodeProfile(UINT32 bitsPerSample, UINT32 samplesPerSecond, UINT32 channel,
                                                    UINT32 avgBytesPerSecond);

UINT32 GetAacQualityBytesPerSecond(int quality);

}