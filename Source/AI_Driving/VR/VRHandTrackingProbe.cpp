// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRHandTrackingProbe.h"
#include "AI_Driving.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"
#include "IXRTrackingSystem.h"
#include "VRHandPresenceComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#if WITH_METAXR_HANDS
#include "OculusXRInputFunctionLibrary.h"
#endif

#define LOCTEXT_NAMESPACE "VRHandTrackingProbe"

namespace
{
	/** Spawns the probe, or destroys it if one is already up */
	void ToggleHandProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World)
		{
			Ar.Log(ELogVerbosity::Error, TEXT("vr.HandProbe needs a world. Run it while playing."));
			return;
		}

		for (TActorIterator<AVRHandTrackingProbe> It(World); It; ++It)
		{
			It->Destroy();
			Ar.Log(TEXT("VR hand tracking probe removed."));
			return;
		}

		World->SpawnActor<AVRHandTrackingProbe>();
		Ar.Log(TEXT("VR hand tracking probe spawned. Put your hands in front of the headset."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GHandProbeCommand(
		TEXT("vr.HandProbe"),
		TEXT("Toggles the VR hand tracking probe. Reports whether hand joints reach Unreal over Link."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ToggleHandProbe));
}

AVRHandTrackingProbe::AVRHandTrackingProbe()
{
	PrimaryActorTick.bCanEverTick = true;

	Readout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Readout"));
	SetRootComponent(Readout);
	Readout->SetHorizontalAlignment(EHTA_Center);
	Readout->SetVerticalAlignment(EVRTA_TextCenter);
	Readout->SetWorldSize(2.5f);
	Readout->SetTextRenderColor(FColor::White);
}

void AVRHandTrackingProbe::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FollowCamera();

	FString Report;

	if (GEngine && GEngine->XRSystem.IsValid())
	{
		Report = FString::Printf(TEXT("XR: %s  stereo=%s\n"),
			*GEngine->XRSystem->GetSystemName().ToString(),
			GEngine->XRSystem->IsHeadTrackingAllowed() ? TEXT("on") : TEXT("OFF"));
	}
	else
	{
		Report = TEXT("XR: no tracking system. Not running in VR.\n");
	}

	Report += PollEngineHandTracker();
	Report += PollMetaHandTracker();

	Readout->SetText(FText::FromString(Report));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(31337, 0.0f, FColor::Yellow, Report);
	}

	// the readout updates every frame, but a log line every frame would be unreadable
	LogCountdown -= DeltaTime;

	if (LogCountdown <= 0.0f)
	{
		LogCountdown = 1.0f;
		UE_LOG(LogAI_Driving, Log, TEXT("[HandProbe]\n%s"), *Report);
	}
}

void AVRHandTrackingProbe::FollowCamera()
{
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);

	if (!CameraManager)
	{
		return;
	}

	// park the readout at arm's length in front of the driver, turned to face them
	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector ReadoutLocation = CameraLocation + CameraManager->GetCameraRotation().Vector() * 80.0f;

	SetActorLocation(ReadoutLocation);
	SetActorRotation((ReadoutLocation - CameraLocation).Rotation());
}

FString AVRHandTrackingProbe::PollEngineHandTracker()
{
	IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();

	if (!Tracker)
	{
		// nothing registered the feature, so the OpenXRHandTracking plugin is off or its
		// extension wasn't offered by the runtime at startup
		return TEXT("ENGINE (XR_EXT_hand_tracking): no hand tracker registered.\n");
	}

	if (!Tracker->IsHandTrackingStateValid())
	{
		// the plugin loaded but the runtime never advertised hand tracking. Over Link this is
		// the answer that matters: the headset isn't passing hands through to the PC
		return FString::Printf(TEXT("ENGINE (%s): registered, but hand tracking UNAVAILABLE.\n"),
			*Tracker->GetHandTrackerDeviceTypeName().ToString());
	}

	FString Report = FString::Printf(TEXT("ENGINE (%s): available.\n"),
		*Tracker->GetHandTrackerDeviceTypeName().ToString());

	const EControllerHand Hands[] = { EControllerHand::Left, EControllerHand::Right };
	const FColor Colors[] = { FColor::Cyan, FColor::Green };

	for (int32 Index = 0; Index < 2; ++Index)
	{
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<float> Radii;
		bool bIsTracked = false;

		const TCHAR* Label = (Index == 0) ? TEXT("  L") : TEXT("  R");

		if (!Tracker->GetAllKeypointStates(Hands[Index], Positions, Rotations, Radii, bIsTracked))
		{
			// the extension is there but no joint poses have ever arrived for this hand
			Report += FString::Printf(TEXT("%s: no joint data.\n"), Label);
			continue;
		}

		Report += FString::Printf(TEXT("%s: %d joints, tracked=%s\n"),
			Label, Positions.Num(), bIsTracked ? TEXT("YES") : TEXT("no (hands out of view?)"));

		if (bIsTracked)
		{
			DrawKeypoints(Hands[Index], Colors[Index]);
		}
	}

	return Report;
}

FString AVRHandTrackingProbe::PollMetaHandTracker()
{
#if WITH_METAXR_HANDS
	if (!UOculusXRInputFunctionLibrary::IsHandTrackingEnabled())
	{
		return TEXT("META (OVRPlugin): hand tracking not enabled.\n");
	}

	FString Report = TEXT("META (OVRPlugin): enabled.\n");

	const EOculusXRHandType Hands[] = { EOculusXRHandType::HandLeft, EOculusXRHandType::HandRight };

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bPositionValid = UOculusXRInputFunctionLibrary::IsHandPositionValid(Hands[Index]);
		const EOculusXRTrackingConfidence Confidence = UOculusXRInputFunctionLibrary::GetTrackingConfidence(Hands[Index]);

		Report += FString::Printf(TEXT("%s: pos=%s confidence=%s\n"),
			(Index == 0) ? TEXT("  L") : TEXT("  R"),
			bPositionValid ? TEXT("valid") : TEXT("INVALID"),
			(Confidence == EOculusXRTrackingConfidence::High) ? TEXT("high") : TEXT("low"));
	}

	return Report;
#else
	return TEXT("META (OVRPlugin): Meta XR plugin not compiled in.\n");
#endif
}

void AVRHandTrackingProbe::DrawKeypoints(EControllerHand Hand, const FColor& Color)
{
	IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();

	if (!Tracker)
	{
		return;
	}

	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<float> Radii;
	bool bIsTracked = false;

	if (!Tracker->GetAllKeypointStates(Hand, Positions, Rotations, Radii, bIsTracked))
	{
		return;
	}

	// keypoints come back already transformed into world space, so a working hand shows up
	// as a constellation of dots sitting on the real one
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const float Radius = Radii.IsValidIndex(Index) && Radii[Index] > 0.0f ? Radii[Index] : 0.5f;
		DrawDebugSphere(GetWorld(), Positions[Index], Radius, 6, Color, false, -1.0f, 0, 0.1f);
	}
}

#undef LOCTEXT_NAMESPACE
