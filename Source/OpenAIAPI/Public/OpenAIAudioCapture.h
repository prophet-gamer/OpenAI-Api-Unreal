#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureComponent.h"
#include "OpenAIAudioCapture.generated.h"

// Delegate for broadcasting captured audio buffer
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioBufferCaptured, const TArray<float>&, AudioBuffer);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENAIAPI_API UOpenAIAudioCapture : public UAudioCaptureComponent
{
    GENERATED_BODY()

public:
    UOpenAIAudioCapture(const FObjectInitializer& ObjectInitializer);

    // Delegate to broadcast captured audio buffer
    UPROPERTY(BlueprintAssignable, Category = "OpenAI|Audio")
    FOnAudioBufferCaptured OnAudioBufferCaptured;

protected:
    virtual bool Init(int32& SampleRate) override;
    virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
    TArray<float> AudioBuffer;
};
