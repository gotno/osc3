#include "SVGWidget.h"

#include "Blueprint/WidgetTree.h"

#include "DefinitivePainter/Public/Canvas/DPCanvas.h"
#include "DefinitivePainter/Public/Widgets/DPSVG.h"
#include "DefinitivePainter/Public/SVG/DPSVGAsset.h"

// for grabbing the color, currently not working
// #include "DefinitivePainter/Public/SVG/Tree/DPSVGRoot.h"
// #include "DefinitivePainter/Public/SVG/Tree/DPSVGElement.h"
// #include "DefinitivePainter/Public/SVG/Tree/DPSVGPaint.h"

#include "osc3GameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include "Engine/Texture2D.h"

void USVGWidget::NativeOnInitialized() {
  Super::NativeOnInitialized();

  dpCanvas = WidgetTree->ConstructWidget<UDPCanvas>(UDPCanvas::StaticClass(), FName("dpCanvas"));
  // gpu accelartion with more than one widget leads to
  // LogTemp: Error: Definitive Painter Context runtime error: Invalid window (BeginRendering)
  // Private/Context/Base/DPContextGPU.cpp:L153
  dpCanvas->EnableGPUAcceleration = false;
  dpCanvas->OneTimeRender = true;
  dpCanvas->ClearColor = FLinearColor::Transparent;

  dpSvgWidget = NewObject<UDPSVG>(this, UDPSVG::StaticClass());
  dpCanvas->AddChild(dpSvgWidget);

  WidgetTree->RootWidget = dpCanvas;
}

void USVGWidget::SetSVG(UDPSVGAsset* svgAsset) {
  dpSvgWidget->SVG = svgAsset;
}

UTexture2D* USVGWidget::GetTexture() {
  return dpCanvas->GetCanvasTexture();
}