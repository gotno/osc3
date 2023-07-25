#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VCV.h"

#include "WidgetSurrogate.generated.h"

class Aosc3GameModeBase;
class UWidgetComponent;
class UTexture2D;
class UDPSVGAsset;

UCLASS()
class OSC3_API AWidgetSurrogate : public AActor {
	GENERATED_BODY()
	
public:	
	AWidgetSurrogate();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
  void SetSVG(UDPSVGAsset* svgAsset, Vec2 size, FString filepath);

private:
  Aosc3GameModeBase* gameMode;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
  USceneComponent* SceneComponent;

  UPROPERTY(VisibleAnywhere)
  UWidgetComponent* WidgetComponent;
  UPROPERTY()
  UMaterialInterface* WidgetMaterialInterface;

  UPROPERTY()
  UTexture2D* texture;

  FString svgFilepath;
  float drawSizeScale{150.f};
};