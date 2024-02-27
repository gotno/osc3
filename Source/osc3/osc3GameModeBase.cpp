#include "osc3GameModeBase.h"
#include "osc3PlayerController.h"
#include "OSCController.h"
#include "RackManager.h"
#include "Player/VRAvatar.h"

#include "VCVModule.h"
#include "VCVCable.h"
#include "ModuleComponents/ContextMenu.h"
#include "ModuleComponents/VCVParam.h"
#include "ModuleComponents/VCVPort.h"
#include "Library.h"
#include "SVG/WidgetSurrogate.h"

#include "Engine/Texture2D.h"
#include "DefinitivePainter/Public/SVG/DPSVGAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include "HeadMountedDisplayFunctionLibrary.h"

Aosc3GameModeBase::Aosc3GameModeBase() {
  PlayerControllerClass = Aosc3PlayerController::StaticClass();

  OSCctrl = CreateDefaultSubobject<AOSCController>(FName(TEXT("OSCctrl")));
  rackman = CreateDefaultSubobject<URackManager>(FName(TEXT("rackman")));
}

void Aosc3GameModeBase::BeginPlay() {
	Super::BeginPlay();

  UHeadMountedDisplayFunctionLibrary::EnableHMD(true);

  rackman->Run();
  // TODO: it'd be nice if this happened after rack was confirmed open
  OSCctrl->Init();

  PlayerController = Cast<Aosc3PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
  PlayerPawn = Cast<AVRAvatar>(UGameplayStatics::GetPlayerPawn(this, 0));
  
  SpawnLibrary();
}

void Aosc3GameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  Super::EndPlay(EndPlayReason);

  rackman->Cleanup();
}

void Aosc3GameModeBase::RequestExit() {
  OSCctrl->SendAutosaveAndExit();
  FTimerHandle hExit;
  GetWorld()->GetTimerManager().SetTimer(
    hExit,
    this,
    &Aosc3GameModeBase::Exit,
    1.f, // 1 second
    false // loop
  );
}

void Aosc3GameModeBase::Exit() {
  UKismetSystemLibrary::QuitGame(
    GetWorld(),
    UGameplayStatics::GetPlayerController(this, 0),
    EQuitPreference::Quit,
    false
  );
}

void Aosc3GameModeBase::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void Aosc3GameModeBase::SpawnModule(VCVModule vcv_module) {
  if (ModuleActors.Contains(vcv_module.id)) return;

  FVector location;
  FRotator rotation;

  if (vcv_module.returnId == 0 && ModuleActors.Contains(LastClickedMenuModuleId)) {
    AVCVModule* lastClickedModule = ModuleActors[LastClickedMenuModuleId];
    lastClickedModule->GetModuleLandingPosition(location, rotation, true);
  } else if (ReturnModulePositions.Contains(vcv_module.returnId)) {
    ReturnModulePosition& rmp = ReturnModulePositions[vcv_module.returnId];
    if (rmp.Location.IsZero()) {
      LibraryActor->GetModuleLandingPosition(vcv_module.box.size.x, location, rotation);
    } else {
      location = rmp.Location;
      rotation = rmp.Rotation;
    }
    ReturnModulePositions.Remove(vcv_module.returnId);
  } else { 
    // WTF IS THIS
    location = FVector(0, vcv_module.box.pos.x, -vcv_module.box.pos.y + 140);
    location.Y += vcv_module.box.size.x / 2;
    location.Z += vcv_module.box.size.y / 2;
    rotation = FRotator(0.f);
  }

  AVCVModule* module =
    GetWorld()->SpawnActor<AVCVModule>(
      AVCVModule::StaticClass(),
      location,
      FRotator(0.f)
    );
 
  ModuleActors.Add(vcv_module.id, module);
  module->Init(vcv_module);

  module->SetActorRotation(rotation);

  ProcessSpawnCableQueue();
}

void Aosc3GameModeBase::QueueCableSpawn(VCVCable vcv_cable) {
  cableQueue.Push(vcv_cable);
  ProcessSpawnCableQueue();
}

void Aosc3GameModeBase::ProcessSpawnCableQueue() {
  TArray<VCVCable> spawnedCables;
  
  bool anyUnpersistedCables = CableActors.ContainsByPredicate([](AVCVCable* Cable) {
    return Cable->Id == -1;
  });

  for (VCVCable cable : cableQueue) {
    AVCVCable* matchingUnpersistedCable{nullptr};
    
    if (anyUnpersistedCables) {
      for (AVCVCable* cableToCheck : CableActors) {
        if (cableToCheck->Id != -1) continue;

        AVCVPort* inputPort = cableToCheck->GetPort(PortType::Input);
        if (!inputPort) continue;
        
        if (inputPort->Id == cable.inputPortId && inputPort->Module->Id == cable.inputModuleId) {
          matchingUnpersistedCable = cableToCheck;
          break;
        }
      }
    }

    if (matchingUnpersistedCable) {
      matchingUnpersistedCable->SetId(cable.id);
      spawnedCables.Push(cable);
    } else if (ModuleActors.Contains(cable.inputModuleId) && ModuleActors.Contains(cable.outputModuleId)) {
      SpawnCable(
        cable.id,
        ModuleActors[cable.inputModuleId]->GetPortActor(PortType::Input, cable.inputPortId),
        ModuleActors[cable.outputModuleId]->GetPortActor(PortType::Output, cable.outputPortId)
      );
      spawnedCables.Push(cable);
    } else {
      // UE_LOG(LogTemp, Warning, TEXT("awaiting modules %lld:%lld for cable %lld"), cable.inputModuleId, cable.outputModuleId, cable.id);
    }
  }

  for (VCVCable cable : spawnedCables) {
    cableQueue.RemoveSwap(cable);
  }
}

// spawn unpersisted cable attached to port
AVCVCable* Aosc3GameModeBase::SpawnCable(AVCVPort* Port) {
  AVCVCable* cable =
    GetWorld()->SpawnActor<AVCVCable>(
      AVCVCable::StaticClass(),
      FVector(0, 0, 0),
      FRotator(0, 0, 0)
    );
  cable->SetPort(Port);
  
  CableActors.Push(cable);
  return cable;
}

// spawn complete/persisted cable
void Aosc3GameModeBase::SpawnCable(int64_t& Id, AVCVPort* InputPort, AVCVPort* OutputPort) {
  AVCVCable* cable = SpawnCable(InputPort);
  cable->SetId(Id);
  cable->SetPort(OutputPort);
}

void Aosc3GameModeBase::RegisterCableConnect(AVCVPort* InputPort, AVCVPort* OutputPort) {
  OSCctrl->CreateCable(InputPort->Module->Id, OutputPort->Module->Id, InputPort->Id, OutputPort->Id);
}

void Aosc3GameModeBase::RegisterCableDisconnect(AVCVCable* Cable) {
  OSCctrl->DestroyCable(Cable->Id);
  Cable->SetId(-1);
}

void Aosc3GameModeBase::DestroyCableActor(AVCVCable* Cable) {
  CableActors.Remove(Cable);
  Cable->Destroy();
}

void Aosc3GameModeBase::DuplicateModule(AVCVModule* Module) {
  int returnId = ++currentReturnModuleId;

  FVector location;
  FRotator rotation;
  Module->GetModuleLandingPosition(location, rotation, false);
  ReturnModulePositions.Add(returnId, ReturnModulePosition(location, rotation));

  FString pluginSlug, moduleSlug;
  Module->GetSlugs(pluginSlug, moduleSlug);

  OSCctrl->SendCreateModule(pluginSlug, moduleSlug, returnId);
}

void Aosc3GameModeBase::RequestModuleSpawn(FString PluginSlug, FString ModuleSlug) {
  int returnId = ++currentReturnModuleId;

  ReturnModulePositions.Add(returnId, ReturnModulePosition());
  OSCctrl->SendCreateModule(PluginSlug, ModuleSlug, returnId);
}

void Aosc3GameModeBase::RequestModuleDiff(const int64_t& ModuleId) const {
  OSCctrl->SendModuleDiffRequest(ModuleId);
};

void Aosc3GameModeBase::SetModuleFavorite(FString PluginSlug, FString ModuleSlug, bool bFavorite) {
  OSCctrl->SetModuleFavorite(PluginSlug, ModuleSlug, bFavorite);
  LibraryActor->SetModuleFavorite(PluginSlug, ModuleSlug, bFavorite);
}

void Aosc3GameModeBase::DestroyModule(int64_t ModuleId, bool bSync) {
  if (!ModuleActors.Contains(ModuleId)) return;

  ModuleActors[ModuleId]->Destroy();
  ModuleActors.Remove(ModuleId);

  if (bSync) OSCctrl->SendDestroyModule(ModuleId);
}

void Aosc3GameModeBase::RequestMenu(const FVCVMenu& Menu) const {
  OSCctrl->RequestMenu(Menu);
}

void Aosc3GameModeBase::ClickMenuItem(const FVCVMenuItem& MenuItem) {
  LastClickedMenuModuleId = MenuItem.moduleId;
  OSCctrl->ClickMenuItem(MenuItem);
}

void Aosc3GameModeBase::UpdateMenuItemQuantity(const FVCVMenuItem& MenuItem, const float& Value) const {
  OSCctrl->UpdateMenuItemQuantity(MenuItem, Value);
}

void Aosc3GameModeBase::UpdateLight(int64_t ModuleId, int32 LightId, FLinearColor Color) {
  if (!ModuleActors.Contains(ModuleId)) return;
  ModuleActors[ModuleId]->UpdateLight(LightId, Color);
}

void Aosc3GameModeBase::UpdateParam(int64_t ModuleId, VCVParam& Param) {
  if (!ModuleActors.Contains(ModuleId)) return;
  ModuleActors[ModuleId]->GetParamActor(Param.id)->Update(Param);
}

void Aosc3GameModeBase::SendParamUpdate(int64_t ModuleId, int32 ParamId, float Value) {
  OSCctrl->SendParamUpdate(ModuleId, ParamId, Value);
}

void Aosc3GameModeBase::RegisterSVG(FString Filepath, Vec2 Size) {
  if (Filepath.Compare(FString("")) == 0) return;
  if (SVGAssets.Contains(Filepath)) return;

  UE_LOG(LogTemp, Warning, TEXT("importing svg %s"), *Filepath);

  UDPSVGAsset* svgAsset = NewObject<UDPSVGAsset>(this, UDPSVGAsset::StaticClass());
  SVGImporter.PerformImport(Filepath, svgAsset);
  SVGAssets.Add(Filepath, svgAsset);
  
  FVector surrogateLocation;
  FRotator surrogateRotation;
  PlayerPawn->GetRenderablePosition(surrogateLocation, surrogateRotation);

  AWidgetSurrogate* surrogate = 
    GetWorld()->SpawnActor<AWidgetSurrogate>(
      AWidgetSurrogate::StaticClass(),
      surrogateLocation,
      surrogateRotation
    );
  
  SVGWidgetSurrogates.Add(Filepath, surrogate);
  surrogate->SetSVG(svgAsset, Size, Filepath);
  surrogate->SetActorScale3D(FVector(0.05f, 0.05f, 0.05f));
}

void Aosc3GameModeBase::RegisterTexture(FString Filepath, UTexture2D* Texture) {
  UE_LOG(LogTemp, Warning, TEXT("registering texture %s"), *Filepath);
  
  SVGTextures.Add(Filepath, Texture);
  // use this to short-circuit surrogate destruction to preview rendering of a given svg
  // if (Filepath.Compare(FString("C:/VCV/rack-src/rack-gotno/res/ComponentLibrary/VCVSlider.svg")) == 0) return;
  SVGWidgetSurrogates[Filepath]->Destroy();
  SVGWidgetSurrogates.Remove(Filepath);
}

UTexture2D* Aosc3GameModeBase::GetTexture(FString Filepath) {
  if (!SVGTextures.Contains(Filepath)) return nullptr;
  return SVGTextures[Filepath];
}

void Aosc3GameModeBase::SpawnLibrary() {
  FActorSpawnParameters spawnParams;
  spawnParams.Owner = this;

  LibraryActor =
    GetWorld()->SpawnActor<ALibrary>(
      ALibrary::StaticClass(),
      FVector(0, 0, 100.f),
      FRotator(0.f),
      spawnParams
    );
}

ALibrary* Aosc3GameModeBase::GetLibrary() {
  if (LibraryActor) return LibraryActor;
  return nullptr;
}

void Aosc3GameModeBase::SetLibraryJsonPath(FString& Path) {
  LibraryActor->SetJsonPath(Path);
}

void Aosc3GameModeBase::SubscribeMenuItemSyncedDelegate(AContextMenu* ContextMenu) {
  OSCctrl->OnMenuItemSyncedDelegate.AddUObject(ContextMenu, &AContextMenu::AddMenuItem);
}

void Aosc3GameModeBase::SubscribeMenuSyncedDelegate(AContextMenu* ContextMenu) {
  OSCctrl->OnMenuSyncedDelegate.AddUObject(ContextMenu, &AContextMenu::MenuSynced);
}