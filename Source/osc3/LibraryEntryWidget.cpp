#include "LibraryEntryWidget.h"

#include "osc3GameModeBase.h"
#include "LibraryEntry.h"

#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "Components/Button.h"

#include "Kismet/GameplayStatics.h"

void ULibraryEntryWidget::NativeConstruct() {
  Super::NativeConstruct();

  Button->OnPressed.AddDynamic(this, &ULibraryEntryWidget::RequestModuleSpawn);
}

void ULibraryEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject) {
	ULibraryEntry* entry = Cast<ULibraryEntry>(ListItemObject);
	PluginName->SetText(FText::FromString(entry->PluginName));
	ModuleName->SetText(FText::FromString(entry->ModuleName));
  PluginSlug = entry->PluginSlug;
  ModuleSlug = entry->ModuleSlug;
}

void ULibraryEntryWidget::RequestModuleSpawn() {
  UE_LOG(LogTemp, Warning, TEXT("requesting spawn- %s:%s"), *PluginSlug, *ModuleSlug);

  Aosc3GameModeBase* gm = Cast<Aosc3GameModeBase>(UGameplayStatics::GetGameMode(this));
  if (gm) {
    gm->RequestModuleSpawn(PluginSlug, ModuleSlug);
  }
}