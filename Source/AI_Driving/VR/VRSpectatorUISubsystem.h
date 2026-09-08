// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "VRSpectatorUISubsystem.generated.h"

class UTextureRenderTarget2D;
class FWidgetRenderer;

/**
 *  Puts UI on the monitor without putting it in front of the driver.
 *
 *  A widget added to the viewport reaches the headset. Measured, not assumed: the probe banner
 *  turned up floating in front of the driver. So a demo that wants readouts for the room and a
 *  clear view for the driver cannot simply add a widget and rely on the spectator screen to be
 *  the only place it lands.
 *
 *  This draws the widget into a render target instead, and hands that to the headset's spectator
 *  screen as an overlay on the eye image. The widget is never part of the scene the headset is
 *  given, so there is nothing for the driver to see.
 *
 *  Call ShowSpectatorWidget with whatever the room should read. Outside VR it does nothing, since
 *  without a headset there is no second audience to serve.
 */
UCLASS()
class UVRSpectatorUISubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Draws this widget on the spectator screen only. Replaces whatever was there */
	UFUNCTION(BlueprintCallable, Category = "VR|Spectator")
	bool ShowSpectatorWidget(TSubclassOf<UUserWidget> WidgetClass);

	/** Clears the spectator overlay and hands the screen back to the plain eye image */
	UFUNCTION(BlueprintCallable, Category = "VR|Spectator")
	void HideSpectatorWidget();

	/** Whether an overlay is currently being drawn */
	UFUNCTION(BlueprintPure, Category = "VR|Spectator")
	bool IsShowingSpectatorWidget() const { return SpectatorWidget != nullptr; }

	/** Size the widget is drawn at. Match the monitor rather than the headset: this never goes
	 *  through a lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Spectator")
	FIntPoint DrawSize = FIntPoint(1920, 1080);

	// Begin UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVRSpectatorUISubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;
	// End UTickableWorldSubsystem interface

private:

	/** The widget being drawn. Held so it can be ticked and torn down */
	UPROPERTY()
	TObjectPtr<UUserWidget> SpectatorWidget;

	/** What the spectator screen is handed. Not a scene texture, so it costs a UI draw and no more */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	TSharedPtr<FWidgetRenderer> WidgetRenderer;
	TSharedPtr<SWidget> SlateWidget;
};
