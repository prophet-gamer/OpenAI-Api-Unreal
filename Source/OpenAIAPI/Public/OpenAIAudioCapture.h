#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioCapture.h"
#include "Sound/SampleBufferIO.h"
#include "OpenAIAudioCapture.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioBufferCaptured, const TArray<float>&, AudioBuffer);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OPENAIAPI_API UOpenAIAudioCapture : public UActorComponent
{
    GENERATED_BODY()

public:
    UOpenAIAudioCapture();

    virtual void Activate(bool bReset) override;

    void DestroyAudioCapture();

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void StartCapturing();

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void StopCapturing();

    UPROPERTY(BlueprintAssignable, Category = "Audio")
    FOnAudioBufferCaptured OnAudioBufferCaptured;

private:
    UAudioCapture* AudioCapture;
    TArray<float> AudioBuffer;

    bool bIsCapturing;

    void OnAudioGenerate(const float* InAudio, int32 NumSamples);
};