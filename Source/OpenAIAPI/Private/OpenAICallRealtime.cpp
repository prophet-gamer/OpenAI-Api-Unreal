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
    UE_LOG(LogTemp, Log, TEXT("UOpenAICallRealtime constructed"));
}

UOpenAICallRealtime::~UOpenAICallRealtime()
{
    UE_LOG(LogTemp, Log, TEXT("UOpenAICallRealtime destructor called"));
    StopRealtimeSession();
}

UOpenAICallRealtime* UOpenAICallRealtime::OpenAICallRealtime(const FString& Instructions, EOAOpenAIVoices Voice)
{
    UOpenAICallRealtime* Node = NewObject<UOpenAICallRealtime>();
    Node->SessionInstructions = Instructions;
    Node->SelectedVoice = Voice;
    UE_LOG(LogTemp, Log, TEXT("OpenAICallRealtime created with instructions: %s and voice: %d"), *Instructions, static_cast<int>(Voice));
    return Node;
}

void UOpenAICallRealtime::Activate()
{
    UE_LOG(LogTemp, Log, TEXT("UOpenAICallRealtime::Activate called"));
    StartRealtimeSession();
}

void UOpenAICallRealtime::StartRealtimeSession()
{
    UE_LOG(LogTemp, Log, TEXT("StartRealtimeSession called"));
    InitializeWebSocket();

    // Create and initialize the audio capture component
    AudioCaptureComponent = NewObject<UOpenAIAudioCapture>(this);
    AudioCaptureComponent->RegisterComponent();
    UE_LOG(LogTemp, Log, TEXT("AudioCaptureComponent created and registered"));

    // Bind the audio buffer captured event
    AudioCaptureComponent->OnAudioBufferCaptured.AddDynamic(
        this, &UOpenAICallRealtime::OnAudioBufferCaptured);
    UE_LOG(LogTemp, Log, TEXT("OnAudioBufferCaptured event bound"));

    // Start capturing audio
    AudioCaptureComponent->Start();
    UE_LOG(LogTemp, Log, TEXT("Audio capture started"));
}

void UOpenAICallRealtime::StopRealtimeSession()
{
    UE_LOG(LogTemp, Log, TEXT("StopRealtimeSession called"));
    AsyncTask(ENamedThreads::GameThread, [this]()
    {
        UE_LOG(LogTemp, Log, TEXT("Executing StopRealtimeSession on GameThread"));
        // Stop capturing audio
        if (AudioCaptureComponent)
        {
            UE_LOG(LogTemp, Log, TEXT("Stopping AudioCaptureComponent"));
            AudioCaptureComponent->Stop();
            AudioCaptureComponent->OnAudioBufferCaptured.RemoveAll(this);
            AudioCaptureComponent->UnregisterComponent();
            AudioCaptureComponent = nullptr;
            UE_LOG(LogTemp, Log, TEXT("AudioCaptureComponent stopped and nullified"));
        }

        // Close WebSocket connection
        if (WebSocket.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("Closing WebSocket"));
            WebSocket->Close();
            WebSocket.Reset();
            UE_LOG(LogTemp, Log, TEXT("WebSocket closed and reset"));
        }

        // Stop and destroy audio component
        if (AudioComponent)
        {
            UE_LOG(LogTemp, Log, TEXT("Stopping and destroying AudioComponent"));
            AudioComponent->Stop();
            AudioComponent->DestroyComponent();
            AudioComponent = nullptr;
            UE_LOG(LogTemp, Log, TEXT("AudioComponent stopped, destroyed, and nullified"));
        }

        // Clear generated sound wave
        if (GeneratedSoundWave)
        {
            UE_LOG(LogTemp, Log, TEXT("Removing GeneratedSoundWave from root"));
            GeneratedSoundWave->RemoveFromRoot();
            GeneratedSoundWave = nullptr;
            UE_LOG(LogTemp, Log, TEXT("GeneratedSoundWave removed from root and nullified"));
        }

        UE_LOG(LogTemp, Log, TEXT("Realtime session stopped successfully"));
    });
}

void UOpenAICallRealtime::InitializeWebSocket()
{
    UE_LOG(LogTemp, Log, TEXT("InitializeWebSocket called"));
    FString ApiKey = UOpenAIUtils::getApiKey();
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API key is not set"));
        OnResponseReceived.Broadcast(TEXT("API key is not set"), false);
        return;
    }

    FString Url = TEXT("wss://api.openai.com/v1/realtime")
                  TEXT("?model=gpt-4o-realtime-preview-2024-10-01");
    UE_LOG(LogTemp, Log, TEXT("WebSocket URL: %s"), *Url);

    // Create a TMap for headers
    TMap<FString, FString> Headers;
    Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    Headers.Add(TEXT("OpenAI-Beta"), TEXT("realtime=v1"));

    // Create the WebSocket with headers
    WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, TEXT(""), Headers);

    if (WebSocket.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("WebSocket created successfully"));
        WebSocket->OnConnected().AddUObject(
            this, &UOpenAICallRealtime::OnWebSocketConnected);
        WebSocket->OnConnectionError().AddUObject(
            this, &UOpenAICallRealtime::OnWebSocketConnectionError);
        WebSocket->OnClosed().AddUObject(
            this, &UOpenAICallRealtime::OnWebSocketClosed);
        WebSocket->OnMessage().AddUObject(
            this, &UOpenAICallRealtime::OnWebSocketMessage);

        WebSocket->Connect();
        UE_LOG(LogTemp, Log, TEXT("WebSocket connection initiated"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create WebSocket"));
        OnResponseReceived.Broadcast(TEXT("Failed to create WebSocket"), false);
    }
}

void UOpenAICallRealtime::OnWebSocketConnected()
{
    UE_LOG(LogTemp, Log, TEXT("WebSocket connected"));
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
    UE_LOG(LogTemp, Log, TEXT("Session update event sent"));

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
    UE_LOG(LogTemp, Log, TEXT("Response create event sent"));

    // Initialize audio playback components
    GeneratedSoundWave = NewObject<USoundWaveProcedural>(this);

    GeneratedSoundWave->SetSampleRate(24000); // OpenAI API uses 24kHz
    GeneratedSoundWave->NumChannels = 1;    // Mono audio
    GeneratedSoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
    GeneratedSoundWave->bLooping = false;
    GeneratedSoundWave->bProcedural = true;

    AudioComponent = NewObject<UAudioComponent>(this);
    AudioComponent->bAutoActivate = false;
    AudioComponent->SetSound(GeneratedSoundWave);
    AudioComponent->RegisterComponent();
    AudioComponent->Play();
    UE_LOG(LogTemp, Log, TEXT("Audio playback components initialized and started"));
}

void UOpenAICallRealtime::OnWebSocketConnectionError(const FString& Error)
{
    UE_LOG(LogTemp, Error, TEXT("WebSocket Connection Error: %s"), *Error);
    OnResponseReceived.Broadcast(
        FString::Printf(TEXT("WebSocket Connection Error: %s"), *Error),
        false);
}

void UOpenAICallRealtime::OnWebSocketClosed(int32 StatusCode,
                                            const FString& Reason,
                                            bool bWasClean)
{
    UE_LOG(LogTemp, Log, TEXT("WebSocket Closed: StatusCode=%d, Reason=%s, WasClean=%d"), StatusCode, *Reason, bWasClean);
    OnResponseReceived.Broadcast(
        FString::Printf(TEXT("WebSocket Closed: %s"), *Reason),
        false);
}

void UOpenAICallRealtime::OnWebSocketMessage(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("WebSocket Message Received: %s"), *Message);
    // Parse and handle incoming messages
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Message);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString EventType = JsonObject->GetStringField(TEXT("type"));
        UE_LOG(LogTemp, Log, TEXT("Event Type: %s"), *EventType);

        if (EventType == TEXT("response.text.delta"))
        {
            // Extract text delta
            FString TextDelta = JsonObject->GetStringField(TEXT("text"));
            UE_LOG(LogTemp, Log, TEXT("Text Delta: %s"), *TextDelta);
            AsyncTask(ENamedThreads::GameThread, [this, TextDelta]()
            {
                  OnResponseReceived.Broadcast(TextDelta, true);
            });
        }
        else if (EventType == TEXT("response.audio.delta"))
        {
            FString AudioBase64 = JsonObject->GetStringField(TEXT("delta"));
            TArray<uint8> AudioData;
            FBase64::Decode(AudioBase64, AudioData);
            UE_LOG(LogTemp, Log, TEXT("Audio Delta received, size: %d bytes"), AudioData.Num());

            PlayAudioData(AudioData);
        }
        else if (EventType == TEXT("error"))
        {
            // Handle error
            TSharedPtr<FJsonObject> ErrorObject =
                JsonObject->GetObjectField(TEXT("error"));
            FString ErrorMessage = ErrorObject->GetStringField(TEXT("message"));
            UE_LOG(LogTemp, Error, TEXT("Error received: %s"), *ErrorMessage);
            OnResponseReceived.Broadcast(ErrorMessage, false);
        }
        // Handle other event types as needed
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse WebSocket message"));
    }
}

void UOpenAICallRealtime::SendRealtimeEvent(
    const TSharedPtr<FJsonObject>& Event)
{
    FString EventString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&EventString);
    FJsonSerializer::Serialize(Event.ToSharedRef(), Writer);

    FString TruncatedEventString = EventString.Left(100);
    if (EventString.Len() > 100)
    {
        TruncatedEventString += TEXT("...");
    }

    UE_LOG(LogTemp, Log, TEXT("Sending Realtime Event: %s"), *TruncatedEventString);

    WebSocket->Send(EventString);
}

void UOpenAICallRealtime::OnAudioBufferCaptured(
    const TArray<float>& AudioBuffer)
{
    UE_LOG(LogTemp, Log, TEXT("Audio Buffer Captured, size: %d samples"), AudioBuffer.Num());
    // Send the audio data to the OpenAI API
    SendAudioDataToAPI(AudioBuffer);

    // Broadcast the audio buffer to Blueprints
    OnAudioBufferReceived.Broadcast(AudioBuffer);
}

void UOpenAICallRealtime::SendAudioDataToAPI(
    const TArray<float>& AudioBuffer)
{
    UE_LOG(LogTemp, Log, TEXT("Sending Audio Data to API, buffer size: %d samples"), AudioBuffer.Num());
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
    UE_LOG(LogTemp, Log, TEXT("Audio data encoded to Base64, length: %d"), Base64Audio.Len());

    // Create the input_audio_buffer.append event
    TSharedPtr<FJsonObject> AudioEvent =
        MakeShareable(new FJsonObject());
    AudioEvent->SetStringField(TEXT("type"),
                               TEXT("input_audio_buffer.append"));
    AudioEvent->SetStringField(TEXT("audio"), Base64Audio);

    // Send the event over WebSocket
    SendRealtimeEvent(AudioEvent);
    UE_LOG(LogTemp, Log, TEXT("Audio data sent to API"));
}

void UOpenAICallRealtime::PlayAudioData(const TArray<uint8>& AudioData)
{
    UE_LOG(LogTemp, Log, TEXT("Playing Audio Data, size: %d bytes"), AudioData.Num());
    if (GeneratedSoundWave)
    {
        // Enqueue the audio data for playback
        GeneratedSoundWave->QueueAudio(AudioData.GetData(), AudioData.Num());
        UE_LOG(LogTemp, Log, TEXT("Audio data queued for playback"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratedSoundWave is null, cannot play audio data"));
    }
}