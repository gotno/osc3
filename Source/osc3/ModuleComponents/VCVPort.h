#pragma once

#include "osc3.h"
#include "VCVData/VCV.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VCVPort.generated.h"

class Aosc3GameModeBase;
class UTexture2D;

class AVCVCable;
class AVCVModule;

UCLASS()
class OSC3_API AVCVPort : public AActor {
	GENERATED_BODY()
	
public:	
	AVCVPort();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
  void Init(struct VCVPort* Model);
  
  int32 Id{-1};
  AVCVModule* Module;

  PortType Type;
  void AddCable(AVCVCable* Cable);
  void RemoveCable(AVCVCable* Cable);
  bool CanConnect(PortType Type);
  bool HasCables();
  AVCVCable* GetTopCable();
  void TriggerCableUpdates();

  void GetTooltipText(FString& Name, FString& Description);
private:
  UPROPERTY(VisibleAnywhere)
  UStaticMeshComponent* StaticMeshComponent;
  UPROPERTY()
  UStaticMesh* StaticMesh;

  UPROPERTY()
  UMaterialInstanceDynamic* BaseMaterialInstance;
  UPROPERTY()
  UMaterialInterface* BaseMaterialInterface;
  UPROPERTY()
  UMaterialInstanceDynamic* FaceMaterialInstance;
  UPROPERTY()
  UMaterialInterface* FaceMaterialInterface;

  UPROPERTY()
  UTexture2D* Texture;
  
  Aosc3GameModeBase* GameMode;

  VCVPort* Model;

  UPROPERTY()
  TArray<AVCVCable*> Cables;
  
  UPROPERTY()
  TArray<AVCVCable*> AttachedCableActors;
};
