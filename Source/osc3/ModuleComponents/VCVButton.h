#pragma once

#include "CoreMinimal.h"
#include "ModuleComponents/VCVParam.h"
#include "VCVButton.generated.h"

class UTexture2D;
class Aosc3GameModeBase;

UCLASS()
class OSC3_API AVCVButton : public AVCVParam {
	GENERATED_BODY()
    
public:
  AVCVButton();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
  void Init(VCVParam* vcv_param) override;

private:
  Aosc3GameModeBase* GameMode;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
  UStaticMeshComponent* MeshComponent;
  UPROPERTY()
  UStaticMesh* StaticMesh;
  
  UPROPERTY()
  UMaterialInstanceDynamic* BaseMaterialInstance;
  UPROPERTY()
  UMaterialInstanceDynamic* FaceMaterialInstance;
  UPROPERTY()
  UMaterialInterface* BaseMaterialInterface;
  UPROPERTY()
  UMaterialInterface* FaceMaterialInterface;

  TCHAR* MeshReference = TEXT("/Script/Engine.StaticMesh'/Game/meshes/faced/unit_button_faced.unit_button_faced'");
  TCHAR* BaseMaterialReference = TEXT("/Script/Engine.Material'/Game/meshes/faced/generic_base.generic_base'");
  TCHAR* FaceMaterialReference = TEXT("/Script/Engine.Material'/Game/meshes/faced/texture_face_bg.texture_face_bg'");
  
  UPROPERTY()
  TArray<UTexture2D*> Frames;
  bool bAllFramesFound{false};
public:
  void Engage() override;
  void Release() override;

  void Update(VCVParam& vcv_param) override;
};
