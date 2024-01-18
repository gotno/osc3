#include "ModuleComponents/VCVSlider.h"

#include "osc3.h"
#include "VCVData/VCV.h"
#include "osc3GameModeBase.h"

#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AVCVSlider::AVCVSlider() {
  BaseMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
  
  // base mesh
  static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshBase(BaseMeshReference);
  if (MeshBase.Object) BaseMeshComponent->SetStaticMesh(MeshBase.Object);
  SetRootComponent(BaseMeshComponent);

  // base materials
  static ConstructorHelpers::FObjectFinder<UMaterial> BaseMaterial(BaseMaterialReference);
  if (BaseMaterial.Object) {
    BaseMaterialInterface = Cast<UMaterial>(BaseMaterial.Object);
  }
  static ConstructorHelpers::FObjectFinder<UMaterial> BaseFaceMaterial(BaseFaceMaterialReference);
  if (BaseFaceMaterial.Object) {
    BaseFaceMaterialInterface = Cast<UMaterial>(BaseFaceMaterial.Object);
  }

  HandleMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle Mesh"));
  HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  
  // handle mesh
  static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshHandle(HandleMeshReference);
  if (MeshHandle.Object) HandleMeshComponent->SetStaticMesh(MeshHandle.Object);
  HandleMeshComponent->SetupAttachment(GetRootComponent());

  HandleMeshComponent->SetGenerateOverlapEvents(true);
  HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  HandleMeshComponent->SetCollisionObjectType(PARAM_OBJECT);
  HandleMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
  HandleMeshComponent->SetCollisionResponseToChannel(LIGHT_OBJECT, ECollisionResponse::ECR_Overlap);
  HandleMeshComponent->SetCollisionResponseToChannel(INTERACTOR_OBJECT, ECollisionResponse::ECR_Overlap);

  // handle materials
  static ConstructorHelpers::FObjectFinder<UMaterial> HandleMaterial(HandleMaterialReference);
  if (HandleMaterial.Object) {
    HandleMaterialInterface = Cast<UMaterial>(HandleMaterial.Object);
  }
  static ConstructorHelpers::FObjectFinder<UMaterial> HandleFaceMaterial(HandleFaceMaterialReference);
  if (HandleFaceMaterial.Object) {
    HandleFaceMaterialInterface = Cast<UMaterial>(HandleFaceMaterial.Object);
  }
  
  SetActorEnableCollision(true);
}

void AVCVSlider::BeginPlay() {
	Super::BeginPlay();
	
  if (BaseMaterialInterface) {
    BaseMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterialInterface, this);
    BaseMeshComponent->SetMaterial(0, BaseMaterialInstance);
  }
  if (BaseFaceMaterialInterface) {
    BaseFaceMaterialInstance = UMaterialInstanceDynamic::Create(BaseFaceMaterialInterface, this);
    BaseMeshComponent->SetMaterial(1, BaseFaceMaterialInstance);
  }

  if (HandleMaterialInterface) {
    HandleMaterialInstance = UMaterialInstanceDynamic::Create(HandleMaterialInterface, this);
    HandleMeshComponent->SetMaterial(0, HandleMaterialInstance);
    HandleMaterialInstance->SetVectorParameterValue(FName("Color"), FColor::Black);
  }
  if (HandleFaceMaterialInterface) {
    HandleFaceMaterialInstance = UMaterialInstanceDynamic::Create(HandleFaceMaterialInterface, this);
    HandleMeshComponent->SetMaterial(1, HandleFaceMaterialInstance);
  }
  
  gameMode = Cast<Aosc3GameModeBase>(UGameplayStatics::GetGameMode(this));
}

void AVCVSlider::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
  
  if (!baseTexture) {
    baseTexture = gameMode->GetTexture(model->svgPaths[0]);
    if (baseTexture) BaseFaceMaterialInstance->SetTextureParameterValue(FName("texture"), baseTexture);
  }

  if (!handleTexture) {
    handleTexture = gameMode->GetTexture(model->svgPaths[1]);
    if (handleTexture) HandleFaceMaterialInstance->SetTextureParameterValue(FName("texture"), handleTexture);
  }
}

void AVCVSlider::init(VCVParam* vcv_param) {
	Super::init(vcv_param);

  HandleMeshComponent->SetWorldScale3D(FVector(RENDER_SCALE, model->handleBox.size.x, model->handleBox.size.y));
  spawnLights(HandleMeshComponent);

  FVector minHandlePosition = GetActorLocation() + FVector(0, model->minHandlePos.x, model->minHandlePos.y);
  FVector maxHandlePosition = GetActorLocation() + FVector(0, model->maxHandlePos.x, model->maxHandlePos.y);
  MaxOffset = model->horizontal
    ? maxHandlePosition.Y - minHandlePosition.Y
    : maxHandlePosition.Z - minHandlePosition.Z;

  WorldOffset = getOffsetFromValue();
  HandleMeshComponent->SetWorldLocation(minHandlePosition + GetSliderDirectionVector() * WorldOffset);
  ShadowOffset = WorldOffset;
}

FVector AVCVSlider::GetSliderDirectionVector() {
  return model->horizontal
    ? HandleMeshComponent->GetRightVector()
    : HandleMeshComponent->GetUpVector();
}

float AVCVSlider::getOffsetFromValue() {
  float valuePercent = (model->value - model->minValue) / (model->maxValue - model->minValue);
  return valuePercent * MaxOffset;
}

float AVCVSlider::getValueFromOffset() {
  float offsetPercent = ShadowOffset / MaxOffset;
  float value = model->minValue + offsetPercent * (model->maxValue - model->minValue);
  if (model->snap) value = round(value);
  return value;
}

void AVCVSlider::engage(FVector ControllerPosition) {
  Super::engage();

  LastControllerPosition = ControllerPosition;
  LastPositionDelta = 0.f;
  LastValue = model->value;
}

void AVCVSlider::alter(FVector ControllerPosition) {
  Super::alter(ControllerPosition);
  if (!engaged) return;

  FVector sliderDirectionVector = GetSliderDirectionVector();
  FVector controllerVector = ControllerPosition - LastControllerPosition; 

  float positionDelta = FVector::DotProduct(controllerVector, sliderDirectionVector);
  positionDelta *= AlterRatio;
  positionDelta =
    FMath::WeightedMovingAverage(positionDelta, LastPositionDelta, 0.2f);
  LastPositionDelta = positionDelta;

  ShadowOffset = FMath::Clamp(ShadowOffset + positionDelta, 0.f, MaxOffset);

  setValue(getValueFromOffset());
  FVector zeroPosition = HandleMeshComponent->GetComponentLocation() - sliderDirectionVector * WorldOffset;
  WorldOffset = model->snap ? getOffsetFromValue() : ShadowOffset;
  HandleMeshComponent->SetWorldLocation(zeroPosition + sliderDirectionVector * WorldOffset);
  
  LastControllerPosition = ControllerPosition;
}

void AVCVSlider::release() {
  Super::release();
  
  // treat snapping sliders like a switch and increment
  // if they haven't been dragged enough to change value
  if (model->snap && model->value == LastValue) {
    float newValue = model->value + 1;
    if (newValue > model->maxValue) newValue = 0;
    setValue(newValue);
    
    FVector sliderDirectionVector = GetSliderDirectionVector();
    FVector zeroPosition = HandleMeshComponent->GetComponentLocation() - sliderDirectionVector * WorldOffset;
    WorldOffset = getOffsetFromValue();
    HandleMeshComponent->SetWorldLocation(zeroPosition + sliderDirectionVector * WorldOffset);
  }

  LastValue = model->value;
  ShadowOffset = WorldOffset;
}

void AVCVSlider::resetValue() {
  Super::resetValue();

  FVector sliderDirectionVector = GetSliderDirectionVector();
  FVector zeroPosition = HandleMeshComponent->GetComponentLocation() - sliderDirectionVector * WorldOffset;

  WorldOffset = getOffsetFromValue();
  HandleMeshComponent->SetWorldLocation(zeroPosition + sliderDirectionVector * WorldOffset);
  ShadowOffset = WorldOffset;
}

void AVCVSlider::Update(VCVParam& Param) {
  Super::Update(Param);

  FVector sliderDirectionVector = GetSliderDirectionVector();
  FVector zeroPosition = HandleMeshComponent->GetComponentLocation() - sliderDirectionVector * WorldOffset;

  WorldOffset = getOffsetFromValue();
  HandleMeshComponent->SetWorldLocation(zeroPosition + sliderDirectionVector * WorldOffset);
  ShadowOffset = WorldOffset;
}