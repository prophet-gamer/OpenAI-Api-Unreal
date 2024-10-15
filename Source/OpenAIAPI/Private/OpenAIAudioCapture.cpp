#include "OpenAIAudioCapture.h"
#include "Kismet/GameplayStatics.h"

UOpenAIAudioCapture::UOpenAIAudioCapture()
{
    PrimaryComponentTick.bCanEverTick = false;
    AudioCapture = nullptr;
    bIsCapturing = false;
    bAutoActivate = true;
    UE_LOG(LogTemp, Log, TEXT("UOpenAIAudioCapture constructor called"));
}


void UOpenAIAudioCapture::Activate(bool bReset)
{
    UE_LOG(LogTemp, Log, TEXT("UOpenAIAudioCapture Activate called"));
    Super::Activate(bReset);

    // Move initialization logic from BeginPlay to here
    if (!AudioCapture)
    {
        AudioCapture = NewObject<UAudioCapture>(this);
        if (AudioCapture)
        {
            AudioCapture->AddGeneratorDelegate([this](const float* InAudio, int32 NumSamples) {
                this->OnAudioGenerate(InAudio, NumSamples);
            });

            // Start capturing the audio
            AudioCapture->OpenDefaultAudioStream();
            StartCapturing();
            UE_LOG(LogTemp, Log, TEXT("-------------------> AudioCapture started from Activate"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create AudioCapture object"));
        }
    } else {
        UE_LOG(LogTemp, Log, TEXT("AudioCapture is already created"));
    }
}

void UOpenAIAudioCapture::StartCapturing()
{
    if (AudioCapture && !bIsCapturing)
    {
        AudioCapture->StartCapturingAudio();
        bIsCapturing = true;
        UE_LOG(LogTemp, Log, TEXT("AudioCapture started"));
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

void UOpenAIAudioCapture::DestroyAudioCapture() {
    UE_LOG(LogTemp, Log, TEXT("DestroyAudioCapture called"));
    if (AudioCapture)
    {
        AudioCapture->ConditionalBeginDestroy();
        AudioCapture = nullptr;
        UE_LOG(LogTemp, Log, TEXT("AudioCapture destroyed"));
    } else {
        UE_LOG(LogTemp, Log, TEXT("AudioCapture is null"));
    }
}

void UOpenAIAudioCapture::OnAudioGenerate(const float* InAudio, int32 NumSamples)
{
    if (bIsCapturing)
    {
        TArray<float> ResampledAudio;
        for (int32 i = 0; i < NumSamples; i += 2)
        {
            ResampledAudio.Add(InAudio[i]);
        }

        // Append the resampled audio to the buffer
        AudioBuffer.Append(ResampledAudio);

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
