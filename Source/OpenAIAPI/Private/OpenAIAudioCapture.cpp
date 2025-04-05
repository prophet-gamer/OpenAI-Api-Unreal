#include "OpenAIAudioCapture.h"
#include "Kismet/GameplayStatics.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <cmath>

UOpenAIAudioCapture::UOpenAIAudioCapture()
{
    PrimaryComponentTick.bCanEverTick = false;
    AudioCapture = nullptr;
    bIsCapturing = false;
    bAutoActivate = true;
    UE_LOG(LogTemp, Log, TEXT("UOpenAIAudioCapture constructor called"));
}

//UOpenAIAudioCapture::UOpenAIAudioCapture()
//	: LocalNumChannels(1), // Default to mono
//	bIsCapturing(false),
//	LastBroadcastTime(0.0),
//    //PrimaryComponentTick.bCanEverTick = false;
//    AudioCapture(nullptr)
//    //bAutoActivate(true)
//{
//	PrimaryComponentTick.bCanEverTick = true;
//    UE_LOG(LogTemp, Log, TEXT("UOpenAIAudioCapture constructor called"));
//}


void UOpenAIAudioCapture::ProcessAndBroadcastBuffer()
{
    TArray<float> BufferCopy;
    {
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

FString UOpenAIAudioCapture::GetDefaultInputDeviceName()
{
    FString DefaultDeviceName = TEXT("");

    // Initialize COM (required for Windows Audio API)
    CoInitialize(nullptr);

    // Create the device enumerator
    IMMDeviceEnumerator* DeviceEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&DeviceEnumerator);
    if (FAILED(hr) || !DeviceEnumerator)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create device enumerator"));
        return DefaultDeviceName;
    }

    // Get the default audio input device
    IMMDevice* DefaultDevice = nullptr;
    hr = DeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &DefaultDevice);
    if (SUCCEEDED(hr) && DefaultDevice)
    {
        IPropertyStore* PropertyStore;
        if (SUCCEEDED(DefaultDevice->OpenPropertyStore(STGM_READ, &PropertyStore)))
        {
            PROPVARIANT DeviceNameProp;
            PropVariantInit(&DeviceNameProp);

            // Get the device friendly name
            if (SUCCEEDED(PropertyStore->GetValue(PKEY_Device_FriendlyName, &DeviceNameProp)))
            {
                DefaultDeviceName = FString(DeviceNameProp.pwszVal);
                UE_LOG(LogTemp, Log, TEXT("Default Windows Input Device: %s"), *DefaultDeviceName);
            }
            PropVariantClear(&DeviceNameProp);
            PropertyStore->Release();
        }
        DefaultDevice->Release();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get default audio input device"));
    }

    DeviceEnumerator->Release();
    CoUninitialize();

    return DefaultDeviceName;
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

            AudioCapture->OpenDefaultAudioStream();
            StartCapturing();
            UE_LOG(LogTemp, Log, TEXT("-------------------> AudioCapture started from Activate on GameThread"));
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
        if (AudioCapture) {
            UE_LOG(LogTemp, Log, TEXT("AudioCapture is valid"));
            AudioCapture->StartCapturingAudio();
            bIsCapturing = true;
            UE_LOG(LogTemp, Log, TEXT("Audio capture started successfully"));
        } else {
            UE_LOG(LogTemp, Log, TEXT("AudioCapture is null"));
        }
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
    
    if (InAudio && bIsCapturing)
    {
        const int32 TargetSampleRate = 24000; // Target sample rate
        const int32 InputSampleRate = 48000; // Assuming the input sample rate is 48000 Hz
        const int32 DownsampleFactor = InputSampleRate / TargetSampleRate; // Calculate downsample factor
        const int32 LocalNumChannels = 2; // Assuming stereo input
        const int32 SamplesPerChannel = NumSamples / LocalNumChannels;
        const int32 DownsampledSamples = SamplesPerChannel / DownsampleFactor;

        // Process samples in stereo pairs and downsample
        for (int32 i = 0; i < DownsampledSamples; i++)
        {
            // Calculate source index in original buffer
            int32 srcIdx = i * DownsampleFactor * LocalNumChannels;

            // Average the left and right channels for mono conversion
            float monoSample = 0.0f;
            for (int32 j = 0; j < DownsampleFactor; j++)
            {
                // Get left and right channel samples
                float leftSample = InAudio[srcIdx + (j * LocalNumChannels)];
                float rightSample = InAudio[srcIdx + (j * LocalNumChannels) + 1];

                // Average the channels
                monoSample += (leftSample + rightSample) * 0.5f;
            }

            // Average the downsampled values
            monoSample /= DownsampleFactor;

            // Add the mono sample to the buffer
            AudioBuffer.Add(monoSample);
        }

        // Check if we should broadcast the buffer
        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastBroadcastTime >= MaxBufferTime || AudioBuffer.Num() >= MaxBufferSize)
        {
            ProcessAndBroadcastBuffer();
            LastBroadcastTime = CurrentTime;
        }
    }
    
    /*
    if (InAudio && bIsCapturing)
    {
        const int32 DownsampleFactor = 2; // Downsample by factor of 2 (e.g., 48kHz -> 24kHz)
        const int32 LocalNumChannels = 2; // Assuming stereo input
        const int32 SamplesPerChannel = NumSamples / LocalNumChannels;
        const int32 DownsampledSamples = SamplesPerChannel / DownsampleFactor;

        // Process samples in stereo pairs and downsample
        for (int32 i = 0; i < DownsampledSamples; i++)
        {
            // Calculate source index in original buffer
            int32 srcIdx = i * DownsampleFactor * LocalNumChannels;

            // Average the left and right channels for mono conversion
            float monoSample = 0.0f;
            for (int32 j = 0; j < DownsampleFactor; j++)
            {
                // Get left and right channel samples
                float leftSample = InAudio[srcIdx + (j * LocalNumChannels)];
                float rightSample = InAudio[srcIdx + (j * LocalNumChannels) + 1];

                // Average the channels
                monoSample += (leftSample + rightSample) * 0.5f;
            }

            // Average the downsampled values
            monoSample /= DownsampleFactor;

            // Add the mono sample to the buffer
            AudioBuffer.Add(monoSample);
        }

        // Check if we should broadcast the buffer
        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastBroadcastTime >= MaxBufferTime || AudioBuffer.Num() >= MaxBufferSize)
        {
            ProcessAndBroadcastBuffer();
            LastBroadcastTime = CurrentTime;
        }
    }
    */
}


    /*
    if (InAudio) {
        if (bIsCapturing)
        {
            const int32 DownsampleFactor = 2; // Factor by which to downsample
            const int32 InterpolatedSamples = NumSamples / DownsampleFactor;
    
            for (int32 i = 0; i < InterpolatedSamples - 1; i++)
            {
               // Indices for original samples
                int32 index1 = i * DownsampleFactor;
                int32 index2 = index1 + DownsampleFactor;

                // Boundary check to prevent accessing out-of-range samples
                if (index2 >= NumSamples)
                {
                    // Handle the last sample if NumSamples is not perfectly divisible
                    index2 = NumSamples - 1;
                }

                // Original sample values
                float sample1 = InAudio[index1];
                float sample2 = InAudio[index2];

                // Calculate the interpolated sample using linear interpolation
                float interpolatedSample = sample1 + ((sample2 - sample1) * 0.5f);

                // Add the interpolated sample to the AudioBuffer
                AudioBuffer.Add(interpolatedSample);
            }
            double CurrentTime = FPlatformTime::Seconds();
            if (CurrentTime - LastBroadcastTime >= MaxBufferTime || AudioBuffer.Num() >= MaxBufferSize)
            {
                ProcessAndBroadcastBuffer();
                LastBroadcastTime = CurrentTime;
            }

        } else {
            UE_LOG(LogTemp, Log, TEXT("bIsCapturing is false"));
        }
    } else {
        UE_LOG(LogTemp, Log, TEXT("InAudio is null"));
    }
    */
