// OpenAICallRealtime.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenAIDefinitions.h"
#include "IWebSocket.h"
#include "Components/AudioComponent.h"
#include "AudioCaptureComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "OpenAICallRealtime.generated.h"

// Delegate for receiving text responses
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnRealtimeResponseReceivedPin,
    const FString&, Response,
    bool, bSuccess);

// Delegate for receiving audio buffers for lip-sync
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnAudioBufferReceived,
    const TArray<float>&, AudioBuffer);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnAudioDataReceived,
    const TArray<uint8>&, AudioData);

UCLASS()
class OPENAIAPI_API UOpenAICallRealtime : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UOpenAICallRealtime();
    ~UOpenAICallRealtime();

    // Static factory function
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "OpenAI")
    static UOpenAICallRealtime* OpenAICallRealtime(const FString& Instructions, EOAOpenAIVoices Voice);

    UPROPERTY(BlueprintAssignable, Category = "OpenAI|Realtime")
    FOnAudioDataReceived OnAudioDataReceived;

    // Override Activate function
    virtual void Activate() override;

    // Stop the realtime session
    UFUNCTION(BlueprintCallable, Category = "OpenAI")
    void StopRealtimeSession();

    // Delegate called when a text response is received
    UPROPERTY(BlueprintAssignable, Category = "OpenAI|Realtime")
    FOnRealtimeResponseReceivedPin OnResponseReceived;

    // Delegate called when an audio buffer is captured
    UPROPERTY(BlueprintAssignable, Category = "OpenAI|Realtime")
    FOnAudioBufferReceived OnAudioBufferReceived;

private:
    // WebSocket connection
    TSharedPtr<IWebSocket> WebSocket;

    // Audio capture component
    UPROPERTY()
    class UOpenAIAudioCapture* AudioCaptureComponent;

    // Procedural sound wave for playing received audio
    UPROPERTY()
    USoundWaveProcedural* GeneratedSoundWave;

    // Audio component for playback
    UPROPERTY()
    UAudioComponent* AudioComponent;

    // Instructions and voice selection
    FString SessionInstructions;
    EOAOpenAIVoices SelectedVoice;

    // Initialize WebSocket connection
    void InitializeWebSocket();

    // Start the realtime session
    void StartRealtimeSession();

    // WebSocket event handlers
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode,
                           const FString& Reason,
                           bool bWasClean);
    void OnWebSocketMessage(const FString& Message);

    // Send event to OpenAI Realtime API
    void SendRealtimeEvent(const TSharedPtr<FJsonObject>& Event);

    // Send audio data to OpenAI API
    void SendAudioDataToAPI(const TArray<float>& AudioBuffer);

    // Handle captured audio buffer
    UFUNCTION()
    void OnAudioBufferCaptured(const TArray<float>& AudioBuffer);

    // Play received audio data
    void PlayAudioData(const TArray<uint8>& AudioData);
};
