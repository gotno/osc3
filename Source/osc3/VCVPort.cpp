#include "VCVPort.h"
#include "VCV.h"

AVCVPort::AVCVPort() {
	PrimaryActorTick.bCanEverTick = true;

  BaseMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Static Mesh"));
  RootComponent = BaseMeshComponent;
  BaseMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  
  static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshBody(TEXT("/Script/Engine.StaticMesh'/Game/meshes/unit_port.unit_port'"));
  
  if (MeshBody.Object) BaseMeshComponent->SetStaticMesh(MeshBody.Object);

  static ConstructorHelpers::FObjectFinder<UMaterial> BodyMaterial(BodyMaterialReference);
  static ConstructorHelpers::FObjectFinder<UMaterial> HoleMaterial(HoleMaterialReference);
  
  if (BodyMaterial.Object) {
    BodyMaterialInterface = Cast<UMaterial>(BodyMaterial.Object);
  }
  if (HoleMaterial.Object) {
    HoleMaterialInterface = Cast<UMaterial>(HoleMaterial.Object);
  }
}

void AVCVPort::BeginPlay() {
	Super::BeginPlay();

  if (BodyMaterialInterface) {
    BodyMaterialInstance = UMaterialInstanceDynamic::Create(BodyMaterialInterface, this);
    BaseMeshComponent->SetMaterial(0, BodyMaterialInstance);
  }
  if (HoleMaterialInterface) {
    HoleMaterialInstance = UMaterialInstanceDynamic::Create(HoleMaterialInterface, this);
    BaseMeshComponent->SetMaterial(1, HoleMaterialInstance);
  }
}

void AVCVPort::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void AVCVPort::SetModel(VCVPort* vcv_port) {
  model = vcv_port;
  SetActorScale3D(FVector(1, model->box.size.x, model->box.size.y));

  if (model->type == PortType::Input) {
    BodyMaterialInstance->SetVectorParameterValue(TEXT("Color"), FColor(247, 208, 96, 255));
  } else {
    BodyMaterialInstance->SetVectorParameterValue(TEXT("Color"), FColor(152, 216, 170, 255));
  }
}

