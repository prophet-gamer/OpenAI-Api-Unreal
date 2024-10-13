#include "OpenAIAudioCapture.h"
#include "Kismet/GameplayStatics.h"

UOpenAIAudioCapture::UOpenAIAudioCapture()
{
    PrimaryComponentTick.bCanEverTick = false;
    AudioCapture = nullptr;
}

void UOpenAIAudioCapture::BeginPlay()
{
    Super::BeginPlay();

    // Create new Audio Capture instance
    AudioCapture = NewObject<UAudioCapture>();
    AudioCapture->AddGeneratorDelegate([this](const float* InAudio, int32 NumSamples) {
        this->OnAudioGenerate(InAudio, NumSamples);
    });

    // Start capturing the audio
    AudioCapture->OpenDefaultAudioStream();
    AudioCapture->StartCapturingAudio();
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
    if (AudioCapture)
    {
        AudioCapture->StartCapturingAudio();
    }
}

void UOpenAIAudioCapture::StopCapturing()
{
    if (AudioCapture)
    {
        AudioCapture->StopCapturingAudio();
    }
}

void UOpenAIAudioCapture::OnAudioGenerate(const float* InAudio, int32 NumSamples)
{
    AudioBuffer.Append(InAudio, NumSamples);

    // Create a copy of the buffer to broadcast
    TArray<float> BufferCopy = AudioBuffer;

    // Broadcast the captured audio data
    OnAudioBufferCaptured.Broadcast(BufferCopy);

    // Clear the buffer after broadcasting
    AudioBuffer.Empty();
}