#include "Player/VRMotionController.h"

#include "osc3.h"
#include "osc3GameModeBase.h"
#include "Player/VRAvatar.h"
#include "VCVData/VCV.h"
#include "VCVModule.h"
#include "VCVCable.h"
#include "ModuleComponents/VCVParam.h"
#include "ModuleComponents/VCVPort.h"
#include "UI/Tooltip.h"
#include "Utility/GrabbableActor.h"

#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "DrawDebugHelpers.h"

#include "Kismet/GameplayStatics.h"

AVRMotionController::AVRMotionController() {
	PrimaryActorTick.bCanEverTick = true;
  
  MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
  SetRootComponent(MotionController);
  
  GrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("GrabSphere"));
  GrabSphere->InitSphereRadius(GrabSphereRadius);
  GrabSphere->SetupAttachment(MotionController);
  
  InteractCapsuleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("InteractCapsuleRoot"));
  InteractCapsuleRoot->SetupAttachment(MotionController);

  InteractCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractCapsule"));
  InteractCapsule->InitCapsuleSize(InteractCapsuleRadius, InteractCapsuleHalfHeight);

  TooltipWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Tooltip"));
  TooltipWidgetComponent->SetupAttachment(MotionController);
  
  WidgetInteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionComponent"));
  WidgetInteractionComponent->SetupAttachment(MotionController);
  WidgetInteractionComponent->VirtualUserIndex = 0;

  WidgetInteractionComponent->bShowDebug = false;
  WidgetInteractionComponent->InteractionDistance = 40.f;
}

void AVRMotionController::BeginPlay() {
	Super::BeginPlay();
  
  Avatar = Cast<AVRAvatar>(GetOwner());
  GameMode = Cast<Aosc3GameModeBase>(UGameplayStatics::GetGameMode(this));

  GrabSphere->OnComponentBeginOverlap.AddDynamic(this, &AVRMotionController::HandleGrabberBeginOverlap);
  GrabSphere->OnComponentEndOverlap.AddDynamic(this, &AVRMotionController::HandleGrabberEndOverlap);
  GrabSphere->ComponentTags.Add(TAG_GRABBER);

  InteractCapsule->AddLocalOffset(InteractCapsule->GetUnscaledCapsuleHalfHeight_WithoutHemisphere() * InteractCapsule->GetUpVector());
  InteractCapsule->AttachToComponent(InteractCapsuleRoot, FAttachmentTransformRules::KeepRelativeTransform);
  InteractCapsuleRoot->AddLocalOffset(InteractCapsuleOffset);
  InteractCapsuleRoot->SetWorldRotation(InteractCapsuleRotation);

  InteractCapsule->SetCollisionObjectType(INTERACTOR_OBJECT);

  InteractCapsule->OnComponentBeginOverlap.AddDynamic(this, &AVRMotionController::HandleInteractorBeginOverlap);
  InteractCapsule->OnComponentEndOverlap.AddDynamic(this, &AVRMotionController::HandleInteractorEndOverlap);

  PlayerController = UGameplayStatics::GetPlayerController(this, 0);

  TooltipWidgetComponent->SetWorldScale3D(FVector(0.04f, 0.04f, 0.04f));
  TooltipWidgetComponent->SetWorldRotation(
    Avatar->GetLookAtCameraRotation(TooltipWidgetComponent->GetComponentLocation())
  );
  SetTooltipVisibility(false);
  TooltipWidget = Cast<UTooltip>(TooltipWidgetComponent->GetUserWidgetObject());
}

void AVRMotionController::SetTrackingSource(EControllerHand Hand) {
  MotionController->SetTrackingSource(Hand);

  HandName = Hand == EControllerHand::Left ? "left" : "right";
  WidgetInteractionComponent->PointerIndex = Hand == EControllerHand::Left ? 1 : 0; 
  
  if (Hand == EControllerHand::Left) {
    GrabSphereOffset.Y = -GrabSphereOffset.Y;
  }
  GrabSphere->AddLocalOffset(GrabSphereOffset);
}

void AVRMotionController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

  // grab sphere
  DrawDebugSphere(
    GetWorld(),
    GrabSphere->GetComponentLocation(),
    GrabSphere->GetUnscaledSphereRadius(),
    16,
    FColor::Blue
  );

  // interact capsule
  DrawDebugSphere(
    GetWorld(),
    InteractCapsuleRoot->GetComponentLocation(),
    InteractCapsule->GetUnscaledCapsuleRadius(),
    16,
    FColor::Green
  );
  DrawDebugCapsule(
    GetWorld(),
    InteractCapsule->GetComponentLocation(),
    InteractCapsule->GetUnscaledCapsuleHalfHeight(),
    InteractCapsule->GetUnscaledCapsuleRadius(),
    InteractCapsule->GetComponentRotation().Quaternion(),
    FColor::Blue
  );

  if (TooltipWidgetComponent->IsVisible())
    TooltipWidgetComponent->SetWorldRotation(
      Avatar->GetLookAtCameraRotation(TooltipWidgetComponent->GetComponentLocation())
    );

  if (WidgetInteractionComponent->IsOverHitTestVisibleWidget() && !bIsWidgetInteracting) {
    bIsWidgetInteracting = true;
    Avatar->SetControllerWidgetInteracting(
      MotionController->GetTrackingSource(),
      bIsWidgetInteracting
    );
  } else if (!WidgetInteractionComponent->IsOverHitTestVisibleWidget() && bIsWidgetInteracting) {
    bIsWidgetInteracting = false;
    Avatar->SetControllerWidgetInteracting(
      MotionController->GetTrackingSource(),
      bIsWidgetInteracting
    );
  }

  if (WidgetInteractionComponent->IsOverHitTestVisibleWidget()) {
    FHitResult widgetHit = WidgetInteractionComponent->GetLastHitResult();
    DrawDebugLine(
      GetWorld(),
      GetActorLocation() + GetActorForwardVector() * 5.f,
      GetActorLocation() + GetActorForwardVector() * (widgetHit.Distance - 0.4f),
      FColor::FromHex(TEXT("010101FF"))
    );
  }
}

void AVRMotionController::LogOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherCompomponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
  // UE_LOG(LogTemp, Warning, TEXT("%s overlap %s"), *HandName, *OtherActor->GetActorNameOrLabel());

  if (OtherActor->ActorHasTag(TAG_GRABBABLE)) {
    // UE_LOG(LogTemp, Warning, TEXT("%s hand can grab %s"), *HandName, *OtherActor->GetActorNameOrLabel());
  }

  if (OtherActor->ActorHasTag(TAG_INTERACTABLE_PARAM) || OtherActor->ActorHasTag(TAG_INTERACTABLE_PORT)) {
    // UE_LOG(LogTemp, Warning, TEXT("%s hand can interact %s"), *HandName, *OtherActor->GetActorNameOrLabel());
  }
}

void AVRMotionController::RefreshTooltip() {
  if (ParamActorToInteract) {
    FString label, displayValue;
    Cast<AVCVParam>(ParamActorToInteract)->GetTooltipText(label, displayValue);
    
    if (displayValue.IsEmpty()) {
      TooltipWidget->SetText(label);
    } else {
      TooltipWidget->SetText(label, displayValue, true);
    }
  } else if (DestinationPortActor && OriginPortActor) {
    FString fromName, _fromDescription;
    AVCVPort* fromActor = 
        OriginPortActor->Type == PortType::Output
          ? OriginPortActor
          : DestinationPortActor;
    fromActor->GetTooltipText(fromName, _fromDescription);
    FString toName, _toDescription;
    AVCVPort* toActor = 
        OriginPortActor->Type == PortType::Input
          ? OriginPortActor
          : DestinationPortActor;
    toActor->GetTooltipText(toName, _toDescription);

    TooltipWidget->SetText(
      FString("from: ").Append(fromName),
      FString("to: ").Append(toName)
    );
  } else if (OriginPortActor) {
    FString name, description;
    OriginPortActor->GetTooltipText(name, description);

    if (bIsPortInteracting) {
      FString prefix(
        OriginPortActor->Type == PortType::Input
          ? "to: "
          : "from: "
      );
      TooltipWidget->SetText(prefix.Append(name));
    } else if (description.IsEmpty()) {
      TooltipWidget->SetText(name);
    } else {
      TooltipWidget->SetText(name, description, false, true);
    }
  }
}

void AVRMotionController::HandleInteractorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {
  if (bIsGrabbing || bIsParamInteracting) return;

  if (OtherActor->ActorHasTag(TAG_INTERACTABLE_PARAM) && !bIsPortInteracting) {
    ParamActorToInteract = OtherActor;
    OriginPortActor = nullptr;

    Avatar->SetControllerParamOrPortInteracting(
      MotionController->GetTrackingSource(),
      true
    );

    PlayerController->PlayHapticEffect(HapticEffects.Bump, MotionController->GetTrackingSource());
    
    SetTooltipVisibility(true);
    
    return;
  }
  
  if (OtherActor->ActorHasTag(TAG_INTERACTABLE_PORT)) {
    if (!bIsPortInteracting) {
      Avatar->SetControllerParamOrPortInteracting(
        MotionController->GetTrackingSource(),
        true
      );

      OriginPortActor = Cast<AVCVPort>(OtherActor);
      ParamActorToInteract = nullptr;
      PlayerController->PlayHapticEffect(HapticEffects.Bump, MotionController->GetTrackingSource());
    } else if (HeldCable && Cast<AVCVPort>(OtherActor)->CanConnect(HeldCable->GetHangingType())) {
      DestinationPortActor = Cast<AVCVPort>(OtherActor);
      PlayerController->PlayHapticEffect(HapticEffects.Bump, MotionController->GetTrackingSource());
    }
    
    SetTooltipVisibility(true);
  }
}

void AVRMotionController::StartParamInteract() {
  // UE_LOG(LogTemp, Display, TEXT("%s hand AVRMotionController::StartParamInteract"), *HandName);
  bIsParamInteracting = true;
}

void AVRMotionController::EndParamInteract() {
  // UE_LOG(LogTemp, Display, TEXT("%s hand AVRMotionController::EndParamInteract"), *HandName);
  bIsParamInteracting = false;

  TSet<AActor*> overlappingActors;
  InteractCapsule->GetOverlappingActors(overlappingActors);

  if (!overlappingActors.Contains(ParamActorToInteract)) {
    ParamActorToInteract = nullptr;
    Avatar->SetControllerParamOrPortInteracting(
      MotionController->GetTrackingSource(),
      false
    );
    // UE_LOG(LogTemp, Warning, TEXT("  (from AVRMotionController::EndParamInteract)"));

    SetTooltipVisibility(false);
  }
}

void AVRMotionController::StartPortInteract() {
  // UE_LOG(LogTemp, Display, TEXT("%s start port interact"), *HandName);
  bIsPortInteracting = true;

  AVCVCable* cableOnPort = OriginPortActor->GetTopCable();
  if (cableOnPort) {
    HeldCable = cableOnPort;
    HeldCable->UnsetPort(OriginPortActor->Type);
    RefreshTooltip();
  } else {
    HeldCable = GameMode->SpawnCable(OriginPortActor);
  }
}

void AVRMotionController::EndPortInteract() {
  // UE_LOG(LogTemp, Display, TEXT("%s end port interact"), *HandName);

  if (DestinationPortActor && DestinationPortActor->CanConnect(HeldCable->GetHangingType())) {
    HeldCable->SetPort(DestinationPortActor);
  } else {
    GameMode->DestroyCableActor(HeldCable);
  }

  bIsPortInteracting = false;
  HeldCable = nullptr;

  TSet<AActor*> overlappingActors;
  InteractCapsule->GetOverlappingActors(overlappingActors);

  if (!overlappingActors.Contains(OriginPortActor) && !overlappingActors.Contains(DestinationPortActor)) {
    OriginPortActor = nullptr;
    DestinationPortActor = nullptr;

    Avatar->SetControllerParamOrPortInteracting(
      MotionController->GetTrackingSource(),
      false
    );
    // UE_LOG(LogTemp, Warning, TEXT("  (from AVRMotionController::EndPortInteract)"));

    SetTooltipVisibility(false);
  } else if (overlappingActors.Contains(DestinationPortActor)) {
    OriginPortActor = DestinationPortActor;
    DestinationPortActor = nullptr;
    RefreshTooltip();
  } else {
    DestinationPortActor = nullptr;
    RefreshTooltip();
  }
}

void AVRMotionController::HandleInteractorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
  if (bIsGrabbing || bIsParamInteracting) return;
  // UE_LOG(LogTemp, Warning, TEXT("AVRMotionController::HandleInteractorEndOverlap with %s, bIsParamInteracting %d"), *OtherActor->GetActorNameOrLabel(), bIsParamInteracting);
  
  if (bIsPortInteracting) {
    if (OtherActor->ActorHasTag(TAG_INTERACTABLE_PORT)) {
      DestinationPortActor = nullptr;
      RefreshTooltip();
    }
    return;
  }

  TSet<AActor*> overlappingActors;
  InteractCapsule->GetOverlappingActors(overlappingActors);
  
  bool wasInteracting =
    OtherActor->ActorHasTag(TAG_INTERACTABLE_PARAM) ||
    OtherActor->ActorHasTag(TAG_INTERACTABLE_PORT);
  bool stillInteracting =
    overlappingActors.Contains(ParamActorToInteract) ||
    overlappingActors.Contains(OriginPortActor) ||
    overlappingActors.Contains(DestinationPortActor);
  if (wasInteracting && !stillInteracting) {
    // UE_LOG(LogTemp, Display, TEXT("%s end param/port interact"), *HandName);
    Avatar->SetControllerParamOrPortInteracting(
      MotionController->GetTrackingSource(),
      false
    );
    // UE_LOG(LogTemp, Warning, TEXT("  (from AVRMotionController::HandleInteractorEndOverlap)"));
    
    SetTooltipVisibility(false);
  }
}

void AVRMotionController::HandleGrabberBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {
  if (bIsGrabbing || bIsParamInteracting) return;

  if (OtherActor->ActorHasTag(TAG_GRABBABLE) && Cast<AGrabbableActor>(OtherActor)) {
    // UE_LOG(LogTemp, Display, TEXT("%s AVRMotionController::HandleGrabberBeginOverlap set ActorToGrab %s"), *HandName, *OtherActor->GetActorNameOrLabel());

    ActorToGrab = OtherActor;
    Cast<AGrabbableActor>(OtherActor)->SetHighlighted(true);
    Avatar->SetControllerGrabbing(
      MotionController->GetTrackingSource(),
      true
    );
  }
}

void AVRMotionController::StartGrab() {
  bIsGrabbing = true;
  // UE_LOG(LogTemp, Warning, TEXT("  %s hand AVRMotionController::StartGrab (bIsGrabbing %d)"), *HandName, bIsGrabbing);
  SetTooltipVisibility(false);
  PlayerController->PlayHapticEffect(HapticEffects.Bump, MotionController->GetTrackingSource());
}

void AVRMotionController::EndGrab() {
  bIsGrabbing = false;
  // UE_LOG(LogTemp, Warning, TEXT("  %s hand AVRMotionController::EndGrab (bIsGrabbing %d)"), *HandName, bIsGrabbing);

  TSet<AActor*> overlappingActors;
  GrabSphere->GetOverlappingActors(overlappingActors);

  if (!overlappingActors.Contains(ActorToGrab)) {
    ActorToGrab = nullptr;
    Avatar->SetControllerGrabbing(
      MotionController->GetTrackingSource(),
      false
    );
  } else {
    Cast<AGrabbableActor>(ActorToGrab)->SetHighlighted(true);
  }
}

void AVRMotionController::HandleGrabberEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
  if (OtherActor->ActorHasTag(TAG_GRABBABLE) && Cast<AGrabbableActor>(OtherActor))
    Cast<AGrabbableActor>(OtherActor)->SetHighlighted(false);
  if (bIsGrabbing || bIsParamInteracting) return;

  TSet<AActor*> overlappingActors;
  GrabSphere->GetOverlappingActors(overlappingActors);

  if (OtherActor->ActorHasTag(TAG_GRABBABLE) && !overlappingActors.Contains(ActorToGrab)) {
    // UE_LOG(LogTemp, Warning, TEXT("  %s hand AVRMotionController::HandleGrabberEndOverlap (bIsGrabbing %d)"), *HandName, bIsGrabbing);
    ActorToGrab = nullptr;
    Avatar->SetControllerGrabbing(
      MotionController->GetTrackingSource(),
      false
    );
  }
}

void AVRMotionController::StartWidgetLeftClick() {
  WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
}

void AVRMotionController::WidgetScroll(float ScrollDelta) {
  WidgetInteractionComponent->ScrollWheel(ScrollDelta);
}

void AVRMotionController::EndWidgetLeftClick() {
  WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
}

void AVRMotionController::SetTooltipVisibility(bool bVisible) {
  if (bVisible && !bTooltipEnabled) return;
  TooltipWidgetComponent->SetVisibility(bVisible, true);
  if (bVisible) RefreshTooltip();
}

void AVRMotionController::GetHeldCableEndInfo(FVector& Location, FVector& ForwardVector) {
  Location =
    InteractCapsuleRoot->GetComponentLocation()
      + InteractCapsule->GetUpVector()
      * InteractCapsule->GetUnscaledCapsuleHalfHeight_WithoutHemisphere() * 2;
  ForwardVector = InteractCapsule->GetUpVector();
}
