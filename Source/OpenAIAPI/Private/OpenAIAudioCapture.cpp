#include "OpenAIAudioCapture.h"
#include "Kismet/GameplayStatics.h"

UOpenAIAudioCapture::UOpenAIAudioCapture()
{
    PrimaryComponentTick.bCanEverTick = false;
    AudioCapture = nullptr;
    bIsCapturing = false;
    UE_LOG(LogTemp, Log, TEXT("UOpenAIAudioCapture constructor called"));
}

void UOpenAIAudioCapture::BeginPlay()
{
    Super::BeginPlay();

    // Create new Audio Capture instance
    AudioCapture = NewObject<UAudioCapture>(this);
    if (AudioCapture)
    {
        AudioCapture->AddGeneratorDelegate([this](const float* InAudio, int32 NumSamples) {
            this->OnAudioGenerate(InAudio, NumSamples);
        });

        // Start capturing the audio
        AudioCapture->OpenDefaultAudioStream();
        StartCapturing();
        UE_LOG(LogTemp, Log, TEXT("------------------->AudioCapture started from BeginPlay"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create AudioCapture object"));
    }
}

void UOpenAIAudioCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapturing();

    if (AudioCapture)
    {
        AudioCapture->ConditionalBeginDestroy();
        AudioCapture = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UOpenAIAudioCapture::StartCapturing()
{
    if (AudioCapture && !bIsCapturing)
    {
        AudioCapture->StartCapturingAudio();
        bIsCapturing = true;
    } else {
        UE_LOG(LogTemp, Log, TEXT("AudioCapture is null or already capturing"));
    }
}

void UOpenAIAudioCapture::StopCapturing()
{
    UE_LOG(LogTemp, Log, TEXT("Attempting to stop audio capture"));
    if (AudioCapture)
    {
        UE_LOG(LogTemp, Log, TEXT("AudioCapture is valid"));
        AudioCapture->StopCapturingAudio();
        UE_LOG(LogTemp, Log, TEXT("Audio capture stopped successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AudioCapture is null"));
    }
}

void UOpenAIAudioCapture::OnAudioGenerate(const float* InAudio, int32 NumSamples)
{
    if (bIsCapturing)
    {
        UE_LOG(LogTemp, Log, TEXT("OnAudioGenerate called"));
        AudioBuffer.Append(InAudio, NumSamples);

        // Create a copy of the buffer to broadcast
        TArray<float> BufferCopy = AudioBuffer;

        // Broadcast the captured audio data
        OnAudioBufferCaptured.Broadcast(BufferCopy);

        // Clear the buffer after broadcasting
        AudioBuffer.Empty();
    } else {
        UE_LOG(LogTemp, Log, TEXT("AudioCapture is null or not capturing"));
    }
}
