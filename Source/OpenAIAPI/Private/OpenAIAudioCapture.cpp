#include "OpenAIAudioCapture.h"

UOpenAIAudioCapture::UOpenAIAudioCapture() : UAudioCaptureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    // Set the desired audio settings
    SampleRate = 24000; // OpenAI Realtime API expects 24kHz
    NumChannels = 1;    // Mono audio
}

int32 UOpenAIAudioCapture::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
    // Copy the audio data to the buffer
    AudioBuffer.SetNumUninitialized(NumSamples);
    FMemory::Memcpy(AudioBuffer.GetData(), OutAudio, NumSamples * sizeof(float));

    // Broadcast the captured audio buffer
    OnAudioBufferCaptured.Broadcast(AudioBuffer);
    return NumSamples;
}
