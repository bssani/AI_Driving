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

	/** Draws this widget on the spectator screen only. Replaces whatever was there.
	 *  Returns false if it could not start right now - see RequestSpectatorWidget for the case
	 *  where "right now" is too early */
	UFUNCTION(BlueprintCallable, Category = "VR|Spectator")
	bool ShowSpectatorWidget(TSubclassOf<UUserWidget> WidgetClass);

	/** Asks for this widget and keeps asking until it can be drawn.
	 *
	 *  At BeginPlay there is usually neither a headset nor a player controller yet, and
	 *  ShowSpectatorWidget simply fails - the same timing trap as recentering, which also cannot
	 *  be done from BeginPlay. Anything wanting an overlay up for the whole session wants this.
	 *
	 *  Gives up after a while and says so, rather than retrying silently forever on a machine
	 *  that has no headset attached. */
	UFUNCTION(BlueprintCallable, Category = "VR|Spectator")
	void RequestSpectatorWidget(TSubclassOf<UUserWidget> WidgetClass);

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

	/** How often the overlay is redrawn, in times per second.
	 *
	 *  Deliberately not once per frame. A lap counter and a position change a few times a race and
	 *  a speed readout is unreadable faster than the eye can follow, so redrawing at the headset's
	 *  refresh rate spends most of its work on frames nobody can tell apart. The audience cannot
	 *  see the difference between this and 90; the frame budget can. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Spectator", meta = (ClampMin = "1.0"))
	float RedrawsPerSecond = 15.f;

	// Begin UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVRSpectatorUISubsystem, STATGROUP_Tickables); }

	/** Nothing to do at all while no overlay is up and none is waiting, so do not even be ticked */
	virtual bool IsTickable() const override { return SpectatorWidget != nullptr || PendingWidgetClass != nullptr; }
	virtual void Deinitialize() override;

	/** Puts up whatever the project settings ask for, once the world is actually running */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	// End UTickableWorldSubsystem interface

private:
	/** Keeps trying to start the pending widget. Returns whether anything is still pending */
	void TickPendingRequest(float DeltaTime);

	/** Asked for but not yet drawable */
	UPROPERTY()
	TSubclassOf<UUserWidget> PendingWidgetClass;

	float PendingRetryCountdown = 0.f;
	float PendingElapsed = 0.f;


	/** The widget being drawn. Held so it can be ticked and torn down */
	UPROPERTY()
	TObjectPtr<UUserWidget> SpectatorWidget;

	/** What the spectator screen is handed. Not a scene texture, so it costs a UI draw and no more */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	TSharedPtr<FWidgetRenderer> WidgetRenderer;
	TSharedPtr<SWidget> SlateWidget;

	/** Time owed before the next redraw */
	float RedrawCountdown = 0.f;
};
