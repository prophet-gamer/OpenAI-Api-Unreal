#include "OpenAIAudioCapture.h"

UOpenAIAudioCapture::UOpenAIAudioCapture(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    NumChannels = 1;    // Mono audio
}

bool UOpenAIAudioCapture::Init(int32& SampleRate)
{
    SampleRate = 24000;  // Set the sample rate to 24kHz
    return Super::Init(SampleRate);  // Call the parent class Init
}

int32 UOpenAIAudioCapture::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
    // Copy the audio data to the buffer
    AudioBuffer.SetNumUninitialized(NumSamples);
    FMemory::Memcpy(AudioBuffer.GetData(), OutAudio, NumSamples * sizeof(float));

    // Broadcast the captured audio buffer
    OnAudioBufferCaptured.Broadcast(AudioBuffer);

    // Return the number of samples generated
    return NumSamples;
}
