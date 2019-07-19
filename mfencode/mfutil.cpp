#include "precomp.h"
#include "mfutil.h"

namespace mf
{

// AAC profile levels are for AAC-LC, with specifications defined in ISO/IEC 14496-3:2009.
constexpr UINT32 c_aacProfileL2 = 0x29; // Max 2 channels, 48kHz
constexpr UINT32 c_aacProfileL4 = 0x2a; // Max 5 channels, 48kHz
constexpr UINT32 c_aacProfileL5 = 0x2b; // Max 5 channels, 96kHz

wil::com_ptr<IMFSourceResolver> CreateSourceResolver()
{
    wil::com_ptr<IMFSourceResolver> resolver;
    THROW_IF_FAILED(MFCreateSourceResolver(&resolver));
    return resolver;
}

wil::com_ptr<IMFTranscodeProfile> CreateTranscodeProfile()
{
    wil::com_ptr<IMFTranscodeProfile> profile;
    THROW_IF_FAILED(MFCreateTranscodeProfile(&profile));
    return profile;
}

wil::com_ptr<IMFAttributes> CreateAttributes(UINT32 initialSize)
{
    wil::com_ptr<IMFAttributes> attributes;
    THROW_IF_FAILED(MFCreateAttributes(&attributes, initialSize));
    return attributes;
}

wil::com_ptr<IMFTopology> CreateTopology(IMFMediaSource *source, PCWSTR output, IMFTranscodeProfile *profile)
{
    wil::com_ptr<IMFTopology> topology;
    THROW_IF_FAILED(MFCreateTranscodeTopology(source, output, profile, &topology));
    return topology;
}

wil::com_ptr<IMFMediaSession> CreateMediaSession(IMFAttributes *configuration)
{
    wil::com_ptr<IMFMediaSession> session;
    THROW_IF_FAILED(MFCreateMediaSession(configuration, &session));
    return session;
}

wil::com_ptr<IMFTranscodeProfile> CreateAacTranscodeProfile(UINT32 bitsPerSample, UINT32 samplesPerSecond, UINT32 channels, UINT32 avgBytesPerSecond)
{
    auto profile = CreateTranscodeProfile();
    auto attributes = CreateAttributes(7);
    AttributeHelper helper{attributes};
    helper.Set(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    helper.Set(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
    helper.Set(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplesPerSecond);
    helper.Set(MF_MT_AUDIO_NUM_CHANNELS, channels);
    helper.Set(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytesPerSecond);
    helper.Set(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    helper.Set(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, c_aacProfileL2);
    THROW_IF_FAILED(profile->SetAudioAttributes(attributes.get()));

    auto containerAttributes = CreateAttributes(1);
    AttributeHelper{containerAttributes}.Set(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
    THROW_IF_FAILED(profile->SetContainerAttributes(containerAttributes.get()));
    return profile;
}

[[nodiscard]] unique_mfshutdown_call Startup()
{
    THROW_IF_FAILED(MFStartup(MF_VERSION));
    return {};
}

MediaSource::MediaSource(PCWSTR input)
{
    auto resolver = CreateSourceResolver();
    wil::com_ptr<IUnknown> source;
    MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
    THROW_IF_FAILED(resolver->CreateObjectFromURL(input, MF_RESOLUTION_MEDIASOURCE, nullptr, &objectType, &source));
    m_source = source.query<IMFMediaSource>();
}

MediaAttributes MediaSource::GetAttributes() const
{
    MediaAttributes result;

    wil::com_ptr<IMFPresentationDescriptor> pd;
    THROW_IF_FAILED(m_source->CreatePresentationDescriptor(&pd));
    result.Duration = ookii::chrono::WindowsTimeUnits{AttributeHelper{pd.get()}.GetUINT64(MF_PD_DURATION)};

    wil::com_ptr<IMFStreamDescriptor> stream;
    BOOL selected;
    THROW_IF_FAILED(pd->GetStreamDescriptorByIndex(0, &selected, &stream));
    wil::com_ptr<IMFMediaTypeHandler> handler;
    THROW_IF_FAILED(stream->GetMediaTypeHandler(&handler));
    wil::com_ptr<IMFMediaType> type;
    THROW_IF_FAILED(handler->GetMediaTypeByIndex(0, &type));

    AttributeHelper helper{type.get()};
    result.BitsPerSample = helper.GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE);
    result.SamplesPerSecond = helper.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND);
    result.Channels = helper.GetUINT32(MF_MT_AUDIO_NUM_CHANNELS);

    return result;
}

IMFMediaSource *MediaSource::Get()
{
    return m_source.get();
}

TranscodeSession::TranscodeSession(MediaSource &source, PCWSTR output, UINT32 avgBytesPerSecond)
    : m_session{CreateMediaSession()},
      m_eventSink{SessionEventSink::Create(*this)}
{
    auto sourceAttributes = source.GetAttributes();
    auto profile = CreateAacTranscodeProfile(sourceAttributes.BitsPerSample, sourceAttributes.SamplesPerSecond,
                                             sourceAttributes.Channels, avgBytesPerSecond);

    auto topology = CreateTopology(source.Get(), output, profile.get());
    THROW_IF_FAILED(m_session->SetTopology(0, topology.get()));
    
    wil::com_ptr<IMFClock> clock;
    THROW_IF_FAILED(m_session->GetClock(&clock));
    m_clock = clock.query<IMFPresentationClock>();

    m_waitEvent.create(wil::EventOptions::ManualReset);

    m_duration = sourceAttributes.Duration;
}

void TranscodeSession::Start()
{
    m_eventSink->BeginGetEvent();
    wil::unique_prop_variant startPosition;
    THROW_IF_FAILED(m_session->Start(&GUID_NULL, &startPosition));
}

bool TranscodeSession::Wait(std::chrono::milliseconds timeout)
{
    if (m_waitEvent.wait(static_cast<DWORD>(timeout.count()))) 
    {
        THROW_IF_FAILED(m_result);
        return true;
    }

    return false;
}

ookii::chrono::WindowsTimeUnits TranscodeSession::GetPosition() const
{
    MFTIME time;
    THROW_IF_FAILED(m_clock->GetTime(&time));
    return ookii::chrono::WindowsTimeUnits{time};
}

float TranscodeSession::GetProgress() const
{
    if (m_duration.count() == 0)
    {
        return {};
    }

    return static_cast<float>(GetPosition().count()) / m_duration.count();
}

void TranscodeSession::OnSessionEnded()
{
    THROW_IF_FAILED(m_session->Close());
}

void TranscodeSession::OnSessionClosed()
{
    m_waitEvent.SetEvent();
}

void TranscodeSession::OnError(HRESULT result)
{
    m_result = result;
    m_session->Close();
    m_waitEvent.SetEvent();
}

IMFMediaSession *TranscodeSession::GetMediaSession()
{
    return m_session.get();
}

AttributeHelper::AttributeHelper(IMFAttributes *attributes)
    : m_attributes{attributes}
{
}

AttributeHelper::AttributeHelper(wil::com_ptr<IMFAttributes> &attributes)
    : m_attributes{attributes.get()}
{
}

UINT32 AttributeHelper::GetUINT32(const GUID &key)
{
    UINT32 value;
    THROW_IF_FAILED(m_attributes->GetUINT32(key, &value));
    return value;
}

UINT64 AttributeHelper::GetUINT64(const GUID &key)
{
    UINT64 value;
    THROW_IF_FAILED(m_attributes->GetUINT64(key, &value));
    return value;
}

void AttributeHelper::Set(const GUID &key, const GUID &value)
{
    THROW_IF_FAILED(m_attributes->SetGUID(key, value));
}

void AttributeHelper::Set(const GUID &key, UINT32 value)
{
    THROW_IF_FAILED(m_attributes->SetUINT32(key, value));
}

SessionEventSink::SessionEventSink(ISessionEvents &eventHandler)
    : m_eventHandler{eventHandler}
{
}

wil::com_ptr<SessionEventSink> SessionEventSink::Create(ISessionEvents &eventHandler)
{
    return wil::MakeOrThrow<SessionEventSink>(eventHandler);
}

STDMETHODIMP SessionEventSink::GetParameters(DWORD *, DWORD *)
{
    return E_NOTIMPL;
}

STDMETHODIMP SessionEventSink::Invoke(IMFAsyncResult *result) try
{
    wil::com_ptr<IMFMediaEvent> event;
    THROW_IF_FAILED(m_eventHandler.GetMediaSession()->EndGetEvent(result, &event));
    MediaEventType type;
    THROW_IF_FAILED(event->GetType(&type));
    HRESULT status;
    THROW_IF_FAILED(event->GetStatus(&status));
    THROW_IF_FAILED(status);

    switch (type)
    {
    case MESessionEnded:
        m_eventHandler.OnSessionEnded();
        break;

    case MESessionClosed:
        m_eventHandler.OnSessionClosed();
        break;
    }

    if (type != MESessionClosed)
    {
        BeginGetEvent();
    }

    return S_OK;
}
catch (...)
{
    auto result = wil::ResultFromCaughtException();
    m_eventHandler.OnError(result);
    return result;
}

void SessionEventSink::BeginGetEvent()
{
    THROW_IF_FAILED(m_eventHandler.GetMediaSession()->BeginGetEvent(this, nullptr));
}

}