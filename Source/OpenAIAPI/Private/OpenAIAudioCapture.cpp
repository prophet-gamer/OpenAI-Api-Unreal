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
         bIsCapturing = false;
         ProcessAndBroadcastBuffer(); // Broadcast any remaining data

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
    if (InAudio) {
        if (bIsCapturing)
        {
        FScopeLock Lock(&AudioBufferLock);

        // Downsample by half (320 to 160 bytes)
        for (int32 i = 0; i < NumSamples; i += 2)
        {
            AudioBuffer.Add(InAudio[i]);
        }

        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastBroadcastTime >= MaxBufferTime || AudioBuffer.Num() >= MaxBufferSize)
        {
            AsyncTask(ENamedThreads::GameThread, [this]()
            {
                ProcessAndBroadcastBuffer();
            });
            LastBroadcastTime = CurrentTime;
        }
    }

    } else {
        UE_LOG(LogTemp, Log, TEXT("InAudio is null"));
    }
}

void UOpenAIAudioCapture::ProcessAndBroadcastBuffer()
{
    TArray<float> BufferCopy;
    {
        FScopeLock Lock(&AudioBufferLock);
        if (AudioBuffer.Num() > 0)
        {
            BufferCopy = AudioBuffer;
            AudioBuffer.Empty();
        }
    }

    if (BufferCopy.Num() > 0)
    {
        // Broadcast the captured audio data
        OnAudioBufferCaptured.Broadcast(BufferCopy);
    }
}