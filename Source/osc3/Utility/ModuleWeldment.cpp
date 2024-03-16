#include "Utility/ModuleWeldment.h"

#include "VCVModule.h"

#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"

AModuleWeldment::AModuleWeldment() {
	PrimaryActorTick.bCanEverTick = true;

  RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root Scene Component"));
  SetRootComponent(RootSceneComponent);
}

void AModuleWeldment::BeginPlay() {
	Super::BeginPlay();
}

void AModuleWeldment::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

  DrawDebugSphere(
    GetWorld(),
    GetActorLocation(),
    5.f,
    16,
    FColor::Black
  );
}

void AModuleWeldment::AddModule(AVCVModule* Module) {
  checkf(
    !Modules.Contains(Module),
    TEXT("module can only be in a weldment once")
  );
  checkf(
    !Module->IsInWeldment(),
    TEXT("module can only be in one weldment")
  );
  Modules.Push(Module);
  Module->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
  Module->SetWeldment(this);
}

void AModuleWeldment::EngageGrab(FVector GrabbedLocation, FRotator GrabbedRotation) {
  GrabOffset = GrabbedLocation - GetActorLocation();

  for (AVCVModule* module : Modules)
    module->AddActorWorldOffset(-GrabOffset);
  SetActorLocation(GrabbedLocation);

  LastGrabbedRotation = GrabbedRotation;
  LastGrabbedLocation = GrabbedLocation;
  LastLocationDelta = FVector(0.f, 0.f, 0.f);
}

void AModuleWeldment::AlterGrab(FVector GrabbedLocation, FRotator GrabbedRotation) {
  // return;
  FQuat qFrom = LastGrabbedRotation.Quaternion();
  // TODO: user
  FQuat qTo = FQuat::Slerp(qFrom, GrabbedRotation.Quaternion(), GRABBABLE_ROTATION_SMOOTHING_FACTOR_DEFAULT);
  FQuat qDelta = qTo * qFrom.Inverse();
  
  FVector locationDelta = GrabbedLocation - LastGrabbedLocation;
  locationDelta = UKismetMathLibrary::WeightedMovingAverage_FVector(
    locationDelta,
    LastLocationDelta,
    // TODO: user
    GRABBABLE_LOCATION_SMOOTHING_FACTOR_DEFAULT
  );
  LastLocationDelta = locationDelta;

  AddActorWorldOffset(locationDelta);
  AddActorWorldRotation(qDelta);

  LastGrabbedLocation = GrabbedLocation;
  LastGrabbedRotation = qTo.Rotator();
  
  for (AVCVModule* module : Modules)
    module->TriggerCableUpdates();
}