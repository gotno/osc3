#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VCVDisplay.generated.h"

UCLASS()
class OSC3_API AVCVDisplay : public AActor {
	GENERATED_BODY()
	
public:	
	AVCVDisplay();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
  
  void init(struct VCVDisplay* model);

private:
  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
  UStaticMeshComponent* BaseMeshComponent;
  
  UPROPERTY()
  UStaticMesh* StaticMesh;
  
  UPROPERTY()
  UMaterialInstanceDynamic* MaterialInstance;

  UPROPERTY()
  UMaterialInterface* MaterialInterface;
  
  VCVDisplay* model;
};
