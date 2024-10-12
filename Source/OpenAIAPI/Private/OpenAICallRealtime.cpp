// OpenAICallRealtime.cpp

#include "OpenAICallRealtime.h"
#include "OpenAIUtils.h"
#include "OpenAIAudioCapture.h"
#include "WebSocketsModule.h"
#include "JsonUtilities.h"
#include "Sound/SoundWaveProcedural.h"
#include "AudioDevice.h"
#include "Async/Async.h"

UOpenAICallRealtime::UOpenAICallRealtime()
{
    AudioCaptureComponent = nullptr;
    GeneratedSoundWave = nullptr;
    AudioComponent = nullptr;
}

UOpenAICallRealtime::~UOpenAICallRealtime()
{
    StopRealtimeSession();
}

UOpenAICallRealtime* UOpenAICallRealtime::OpenAICallRealtime(const FString& Instructions, EOAOpenAIVoices Voice)
{
    UOpenAICallRealtime* Node = NewObject<UOpenAICallRealtime>();
    Node->SessionInstructions = Instructions;
    Node->SelectedVoice = Voice;
    return Node;
}

void UOpenAICallRealtime::Activate()
{
    StartRealtimeSession();
}

void UOpenAICallRealtime::StartRealtimeSession()
{
    InitializeWebSocket();

    // Create and initialize the audio capture component
    AudioCaptureComponent = NewObject<UOpenAIAudioCapture>(this);
    AudioCaptureComponent->RegisterComponent();

    // Bind the audio buffer captured event
    AudioCaptureComponent->OnAudioBufferCaptured.AddDynamic(
        this, &UOpenAICallRealtime::OnAudioBufferCaptured);

    // Start capturing audio
    AudioCaptureComponent->Start();
}

void UOpenAICallRealtime::StopRealtimeSession()
{
    // Stop capturing audio
    if (AudioCaptureComponent)
    {
        AudioCaptureComponent->Stop();
        AudioCaptureComponent->OnAudioBufferCaptured.RemoveAll(this);
        AudioCaptureComponent->UnregisterComponent();
        AudioCaptureComponent = nullptr;
    }

    // Close WebSocket connection
    if (WebSocket.IsValid())
    {
        WebSocket->Close();
        WebSocket = nullptr;
    }

    // Stop and destroy audio component
    if (AudioComponent)
    {
        AudioComponent->Stop();
        AudioComponent->DestroyComponent();
        AudioComponent = nullptr;
    }

    // Clear generated sound wave
    if (GeneratedSoundWave)
    {
        GeneratedSoundWave = nullptr;
    }
}

void UOpenAICallRealtime::InitializeWebSocket()
{
    FString ApiKey = UOpenAIUtils::getApiKey();
    if (ApiKey.IsEmpty())
    {
        OnResponseReceived.Broadcast(TEXT("API key is not set"), false);
        return;
    }

    FString Url = TEXT("wss://api.openai.com/v1/realtime")
                  TEXT("?model=gpt-4o-realtime-preview-2024-10-01");

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url);

    TMap<FString, FString> UpgradeHeaders;
    UpgradeHeaders.Add(TEXT("Authorization"),
                       FString::Printf(TEXT("Bearer %s"), *ApiKey));
    UpgradeHeaders.Add(TEXT("OpenAI-Beta"), TEXT("realtime=v1"));

    WebSocket->SetHeaders(UpgradeHeaders);

    WebSocket->OnConnected().AddUObject(
        this, &UOpenAICallRealtime::OnWebSocketConnected);
    WebSocket->OnConnectionError().AddUObject(
        this, &UOpenAICallRealtime::OnWebSocketConnectionError);
    WebSocket->OnClosed().AddUObject(
        this, &UOpenAICallRealtime::OnWebSocketClosed);
    WebSocket->OnMessage().AddUObject(
        this, &UOpenAICallRealtime::OnWebSocketMessage);

    WebSocket->Connect();
}

void UOpenAICallRealtime::OnWebSocketConnected()
{
    // Send session update event to set voice and other configurations
    TSharedPtr<FJsonObject> SessionUpdateEvent =
        MakeShareable(new FJsonObject());
    SessionUpdateEvent->SetStringField(TEXT("type"), TEXT("session.update"));

    TSharedPtr<FJsonObject> ConfigObject =
        MakeShareable(new FJsonObject());
    ConfigObject->SetStringField(
        TEXT("voice"),
        UOpenAIUtils::GetVoiceString(SelectedVoice));
    // Add more configurations if needed

    SessionUpdateEvent->SetObjectField(TEXT("config"), ConfigObject);

    SendRealtimeEvent(SessionUpdateEvent);

    // Create response
    TSharedPtr<FJsonObject> ResponseCreateEvent =
        MakeShareable(new FJsonObject());
    ResponseCreateEvent->SetStringField(TEXT("type"), TEXT("response.create"));

    TSharedPtr<FJsonObject> ResponseObject =
        MakeShareable(new FJsonObject());
    ResponseObject->SetStringField(TEXT("instructions"),
                                   SessionInstructions);

    TArray<TSharedPtr<FJsonValue>> Modalities;
    Modalities.Add(MakeShareable(new FJsonValueString(TEXT("text"))));
    Modalities.Add(MakeShareable(new FJsonValueString(TEXT("audio"))));

    ResponseObject->SetArrayField(TEXT("modalities"), Modalities);

    ResponseCreateEvent->SetObjectField(TEXT("response"), ResponseObject);

    SendRealtimeEvent(ResponseCreateEvent);

    // Initialize audio playback components
    GeneratedSoundWave = NewObject<USoundWaveProcedural>(this);
    GeneratedSoundWave->SampleRate = 24000; // OpenAI API uses 24kHz
    GeneratedSoundWave->NumChannels = 1;    // Mono audio
    GeneratedSoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
    GeneratedSoundWave->bLooping = false;
    GeneratedSoundWave->bProcedural = true;

    AudioComponent = NewObject<UAudioComponent>(this);
    AudioComponent->bAutoActivate = false;
    AudioComponent->SetSound(GeneratedSoundWave);
    AudioComponent->RegisterComponent();
    AudioComponent->Play();
}

void UOpenAICallRealtime::OnWebSocketConnectionError(const FString& Error)
{
    OnResponseReceived.Broadcast(
        FString::Printf(TEXT("WebSocket Connection Error: %s"), *Error),
        false);
}

void UOpenAICallRealtime::OnWebSocketClosed(int32 StatusCode,
                                            const FString& Reason,
                                            bool bWasClean)
{
    OnResponseReceived.Broadcast(
        FString::Printf(TEXT("WebSocket Closed: %s"), *Reason),
        false);
}

void UOpenAICallRealtime::OnWebSocketMessage(const FString& Message)
{
    // Parse and handle incoming messages
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Message);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString EventType = JsonObject->GetStringField(TEXT("type"));

        if (EventType == TEXT("response.text.delta"))
        {
            // Extract text delta
            FString TextDelta = JsonObject->GetStringField(TEXT("text"));
            AsyncTask(ENamedThreads::GameThread, [this, TextDelta]()
            {
                  OnResponseReceived.Broadcast(TextDelta, true);
            });
        }
        else if (EventType == TEXT("response.audio.delta"))
        {
            FString AudioBase64 = JsonObject->GetStringField(TEXT("audio"));
            TArray<uint8> AudioData;
            FBase64::Decode(AudioBase64, AudioData);

            PlayAudioData(AudioData);
        }
        else if (EventType == TEXT("error"))
        {
            // Handle error
            TSharedPtr<FJsonObject> ErrorObject =
                JsonObject->GetObjectField(TEXT("error"));
            FString ErrorMessage = ErrorObject->GetStringField(TEXT("message"));
            OnResponseReceived.Broadcast(ErrorMessage, false);
        }
        // Handle other event types as needed
    }
}

void UOpenAICallRealtime::SendRealtimeEvent(
    const TSharedPtr<FJsonObject>& Event)
{
    FString EventString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&EventString);
    FJsonSerializer::Serialize(Event.ToSharedRef(), Writer);
    WebSocket->Send(EventString);
}

void UOpenAICallRealtime::OnAudioBufferCaptured(
    const TArray<float>& AudioBuffer)
{
    // Send the audio data to the OpenAI API
    SendAudioDataToAPI(AudioBuffer);

    // Broadcast the audio buffer to Blueprints
    OnAudioBufferReceived.Broadcast(AudioBuffer);
}

void UOpenAICallRealtime::SendAudioDataToAPI(
    const TArray<float>& AudioBuffer)
{
    // Convert float audio data to PCM16
    TArray<uint8> PCM16Data;
    PCM16Data.SetNumUninitialized(AudioBuffer.Num() * sizeof(int16));

    for (int32 i = 0; i < AudioBuffer.Num(); ++i)
    {
        float Sample = FMath::Clamp(AudioBuffer[i], -1.0f, 1.0f);
        int16 IntSample = (int16)(Sample * 32767.0f);
        PCM16Data[i * 2] = IntSample & 0xFF;
        PCM16Data[i * 2 + 1] = (IntSample >> 8) & 0xFF;
    }

    // Base64 encode the PCM16 data
    FString Base64Audio = FBase64::Encode(PCM16Data);

    // Create the input_audio_buffer.append event
    TSharedPtr<FJsonObject> AudioEvent =
        MakeShareable(new FJsonObject());
    AudioEvent->SetStringField(TEXT("type"),
                               TEXT("input_audio_buffer.append"));
    AudioEvent->SetStringField(TEXT("audio"), Base64Audio);

    // Send the event over WebSocket
    SendRealtimeEvent(AudioEvent);
}

void UOpenAICallRealtime::PlayAudioData(const TArray<uint8>& AudioData)
{
    if (GeneratedSoundWave)
    {
        // Enqueue the audio data for playback
        GeneratedSoundWave->QueueAudio(AudioData.GetData(), AudioData.Num());
    }
}
