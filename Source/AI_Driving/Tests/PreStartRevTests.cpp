#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI_DrivingPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosBrakeReverseGuardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodyInstance.h"
#include "RaceDirectorSubsystem.h"
#include "RaceParticipantComponent.h"

namespace
{

// Uses the shipped vehicle, torque curve and Chaos simulation, rather than a pedal mock.
struct FPreStartWorld
{
	UWorld* World = nullptr;
	AAI_DrivingPawn* Car = nullptr;
	URaceParticipantComponent* Participant = nullptr;
	URaceDirectorSubsystem* Director = nullptr;
	bool bInitialAutomaticGears = false;

	bool Initialize(FAutomationTestBase& Test)
	{
		UClass* CarClass = LoadClass<AAI_DrivingPawn>(nullptr,
			TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn.BP_SportsCar_Pawn_C"));
		if (!Test.TestNotNull(TEXT("SportsCar Blueprint loads"), CarClass)) return false;

		UWorld::InitializationValues Init;
		Init.CreatePhysicsScene(true).ShouldSimulatePhysics(true).EnableTraceCollision(true)
			.CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::SM5, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		// There is no game mode in this isolated world to dispatch actor BeginPlay.
		World->GetWorldSettings()->NotifyBeginPlay();
		World->GetWorldSettings()->NotifyMatchStarted();

		AActor* Floor = World->SpawnActor<AActor>();
		UStaticMeshComponent* FloorMesh = NewObject<UStaticMeshComponent>(Floor);
		Floor->SetRootComponent(FloorMesh);
		Floor->AddInstanceComponent(FloorMesh);
		FloorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		FloorMesh->SetCollisionProfileName(TEXT("BlockAll"));
		FloorMesh->SetWorldScale3D(FVector(100.f, 100.f, 1.f));
		FloorMesh->SetWorldLocation(FVector(0.f, 0.f, -50.f));
		FloorMesh->RegisterComponent();

		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Car = World->SpawnActor<AAI_DrivingPawn>(CarClass, FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, Spawn);
		if (!Test.TestNotNull(TEXT("SportsCar spawns"), Car)) return false;
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		Controller->PlayerState = World->SpawnActor<APlayerState>();
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Car);
		// The first movement tick finishes setting up the authored transmission. Measure its
		// runtime policy before the neutral guard temporarily changes it.
		UChaosBrakeReverseGuardComponent* Guard = Car->FindComponentByClass<UChaosBrakeReverseGuardComponent>();
		Guard->SetComponentTickEnabled(false);
		Step();
		bInitialAutomaticGears = Car->GetChaosVehicleMovement()->GetUseAutoGears();
		Guard->SetComponentTickEnabled(true);
		TArray<UChaosBrakeReverseGuardComponent*> Guards;
		Car->GetComponents(Guards);
		for (UChaosBrakeReverseGuardComponent* Candidate : Guards)
		{
			Test.AddInfo(FString::Printf(TEXT("Guard component %s; initial auto=%d"), *Candidate->GetName(), bInitialAutomaticGears));
		}

		Participant = Car->FindComponentByClass<URaceParticipantComponent>();
		if (!Participant)
		{
			Participant = NewObject<URaceParticipantComponent>(Car);
			Car->AddInstanceComponent(Participant);
			Participant->bIsPlayer = true;
			Participant->RegisterComponent();
		}
		Director = World->GetSubsystem<URaceDirectorSubsystem>();
		Director->HoldAtGrid();
		return true;
	}

	void Step(int32 Frames = 1)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 1.f / 60.f);
		}
	}

	~FPreStartWorld()
	{
		if (World)
		{
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	}
};

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPreStartRevTest, "AI_Driving.PreStart.RevAndLaunch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPreStartRevTest::RunTest(const FString& Parameters)
{
	FPreStartWorld Fixture;
	if (!Fixture.Initialize(*this)) return false;
	Fixture.Step(90);
	AAI_DrivingPawn* Car = Fixture.Car;
	UChaosWheeledVehicleMovementComponent* Movement = Car->GetChaosVehicleMovement();
	const FVector GridPosition = Car->GetActorLocation();
	const float IdleRPM = Movement->GetEngineRotationSpeed();
	TArray<UChaosBrakeReverseGuardComponent*> Guards;
	Car->GetComponents(Guards);
	int32 EnabledGuards = 0;
	for (const UChaosBrakeReverseGuardComponent* Guard : Guards)
	{
		EnabledGuards += Guard->IsComponentTickEnabled() ? 1 : 0;
	}
	TestEqual(TEXT("Only one gearbox guard controls a vehicle with legacy and native components"), EnabledGuards, 1);
	TestTrue(TEXT("Pawn accepts pedals while held on grid"), Car->InputEnabled());
	TestTrue(TEXT("Vehicle settles down onto the floor"), GridPosition.Z < 130.f && GridPosition.Z > 10.f);

	Car->DoThrottle(0.8f);
	Car->DoBrake(0.5f);
	Fixture.Step(150);
	const float RevRPM = Movement->GetEngineRotationSpeed();
	AddInfo(FString::Printf(TEXT("Idle RPM %.1f; rev RPM %.1f; chassis Z %.1f"), IdleRPM, RevRPM, GridPosition.Z));
	const UChaosBrakeReverseGuardComponent* Guard = Car->FindComponentByClass<UChaosBrakeReverseGuardComponent>();
	AddInfo(FString::Printf(TEXT("Pedal throttle %.3f brake %.3f; player %d local %d; rev allowed %d; auto %d; guard tick %d movement tick %d"),
		Movement->GetThrottleInput(), Movement->GetBrakeInput(), Car->IsPlayerControlled(), Car->IsLocallyControlled(),
		Fixture.Participant->IsPreStartRevvingAllowed(), Movement->GetUseAutoGears(), Guard && Guard->IsComponentTickEnabled(), Movement->IsComponentTickEnabled()));
	TestEqual(TEXT("Pre-start throttle and brake leave gearbox in neutral"), Movement->GetCurrentGear(), 0);
	TestTrue(TEXT("Throttle raises actual engine RPM with brake held"), RevRPM > IdleRPM + 500.f);
	TestTrue(TEXT("Car stays on grid while revving"), FVector::Dist2D(GridPosition, Car->GetActorLocation()) < 0.25);

	Car->DoThrottle(0.f);
	Fixture.Step(150);
	TestTrue(TEXT("Lifting throttle lowers RPM"), Movement->GetEngineRotationSpeed() < RevRPM - 300.f);

	Car->DoBrakeStop();
	Car->DoThrottle(0.8f);
	Fixture.Step(90);
	Fixture.Director->StartRace();
	Fixture.Step(120);
	TestFalse(TEXT("Green light releases hold"), Fixture.Participant->IsInputLocked());
	TestTrue(TEXT("Held throttle launches vehicle at green light"), Car->GetActorLocation().X > GridPosition.X + 100.f);
	TestTrue(TEXT("Driving uses forward gear"), Movement->GetCurrentGear() > 0);
	TestEqual(TEXT("Original transmission mode restored"), Movement->GetUseAutoGears(), Fixture.bInitialAutomaticGears);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPreStartPhysicsHoldTest, "AI_Driving.PreStart.PhysicsHoldAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPreStartPhysicsHoldTest::RunTest(const FString& Parameters)
{
	FPreStartWorld Fixture;
	if (!Fixture.Initialize(*this)) return false;
	Fixture.Step(90);
	FBodyInstance* Body = Fixture.Car->GetMesh()->GetBodyInstance();
	TestTrue(TEXT("Physics solver constrains X/Y instead of a drift dead zone"), Body->bLockXTranslation && Body->bLockYTranslation);
	TestFalse(TEXT("Height remains free for suspension settling"), Body->bLockZTranslation);
	TestTrue(TEXT("Physics solver constrains chassis rotation"), Body->bLockXRotation && Body->bLockYRotation && Body->bLockZRotation);
	const FVector GridPosition = Fixture.Car->GetActorLocation();
	const FQuat GridRotation = Fixture.Car->GetActorQuat();
	double MaxDrift = 0.0;
	double MaxRotation = 0.0;
	Fixture.Car->DoThrottle(1.f);
	Fixture.Car->DoSteering(1.f);
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		if (Frame % 30 == 0) Fixture.Car->GetMesh()->AddImpulse(FVector(20000.f, 10000.f, 0.f));
		Fixture.Step();
		MaxDrift = FMath::Max(MaxDrift, FVector::Dist2D(GridPosition, Fixture.Car->GetActorLocation()));
		MaxRotation = FMath::Max(MaxRotation, FMath::RadiansToDegrees(GridRotation.AngularDistance(Fixture.Car->GetActorQuat())));
	}
	AddInfo(FString::Printf(TEXT("Max grid drift %.4f cm; rotation %.4f deg"), MaxDrift, MaxRotation));
	TestTrue(TEXT("No visible creeping under throttle, steering and impulses"), MaxDrift < 0.25);
	TestTrue(TEXT("No chassis turning on grid"), MaxRotation < 0.25);
	Fixture.Director->StartRace();
	TestFalse(TEXT("X/Y translation flags restored"), Body->bLockXTranslation || Body->bLockYTranslation);
	TestFalse(TEXT("Rotation flags restored"), Body->bLockXRotation || Body->bLockYRotation || Body->bLockZRotation);
	// A vehicle with an existing constraint must recover that exact policy, too.
	Body->bLockZRotation = true;
	Body->SetDOFLock(EDOFMode::SixDOF);
	Fixture.Participant->SetInputLocked(true, true);
	Fixture.Participant->SetInputLocked(false);
	TestTrue(TEXT("Existing rotation constraint survives a hold cycle"), Body->bLockZRotation);
	TestEqual(TEXT("Existing DOF mode survives a hold cycle"), static_cast<int32>(Body->DOFMode.GetValue()), static_cast<int32>(EDOFMode::SixDOF));
	Fixture.Participant->SetInputLocked(true, true);
	Fixture.Participant->DestroyComponent();
	TestFalse(TEXT("Removing participant releases horizontal hold"), Body->bLockXTranslation || Body->bLockYTranslation);
	TestTrue(TEXT("Removing participant preserves original rotation constraint"), Body->bLockZRotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPreStartRestartTest, "AI_Driving.PreStart.RestartAndFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPreStartRestartTest::RunTest(const FString& Parameters)
{
	FPreStartWorld Fixture;
	if (!Fixture.Initialize(*this)) return false;
	Fixture.Step(90);
	Fixture.Car->DoThrottle(1.f);
	Fixture.Director->StartCountdown(3.f);
	Fixture.Step(90);
	TestTrue(TEXT("Countdown retains grid hold"), Fixture.Participant->IsInputLocked());
	TestTrue(TEXT("Countdown allows revving"), Fixture.Participant->IsPreStartRevvingAllowed());
	TestTrue(TEXT("Countdown accepts actual RPM control"), Fixture.Car->GetChaosVehicleMovement()->GetEngineRotationSpeed() > 1200.f);
	Fixture.Step(180);
	TestFalse(TEXT("Countdown green light releases grid"), Fixture.Participant->IsInputLocked());
	TestTrue(TEXT("Countdown green light launches held throttle"), Fixture.Car->GetActorLocation().X > 100.f);

	Fixture.Participant->StopAndHold(10000.f);
	Fixture.Step(150);
	TestTrue(TEXT("Finished vehicle is held"), Fixture.Participant->IsInputLocked());
	TestFalse(TEXT("Finish hold does not permit pre-start revving"), Fixture.Participant->IsPreStartRevvingAllowed());
	const FVector FinishPosition = Fixture.Car->GetActorLocation();
	const float FinishRPM = Fixture.Car->GetChaosVehicleMovement()->GetEngineRotationSpeed();
	Fixture.Car->DoThrottle(1.f);
	Fixture.Step(150);
	TestTrue(TEXT("Finish hold still blocks controller-route throttle"), Fixture.Car->GetChaosVehicleMovement()->GetThrottleInput() < 0.01f);
	TestTrue(TEXT("Finish hold does not rev engine back up"), Fixture.Car->GetChaosVehicleMovement()->GetEngineRotationSpeed() <= FinishRPM + 10.f);
	TestTrue(TEXT("Finish hold stays stationary"), FVector::Dist2D(FinishPosition, Fixture.Car->GetActorLocation()) < 0.25);

	const FTransform NewGrid(FRotator(0.f, 25.f, 0.f), FVector(500.f, 300.f, 150.f));
	Fixture.Director->RegisterGridPlacement(Fixture.Participant, NewGrid, 0, 0.f);
	Fixture.Director->ResetRace();
	Fixture.Step(90);
	TestTrue(TEXT("Restart holds the new grid anchor"), FVector::Dist2D(NewGrid.GetLocation(), Fixture.Car->GetActorLocation()) < 0.25);
	TestTrue(TEXT("Restart re-enables pedal input"), Fixture.Car->InputEnabled());
	TestTrue(TEXT("Restart re-enables pre-start revving"), Fixture.Participant->IsPreStartRevvingAllowed());
	Fixture.Car->DoThrottle(0.8f);
	Fixture.Step(90);
	TestTrue(TEXT("New session can rev again"), Fixture.Car->GetChaosVehicleMovement()->GetEngineRotationSpeed() > 1200.f);
	Fixture.Director->StartRace();
	Fixture.Step(120);
	TestTrue(TEXT("New session launches from its new grid"), FVector::Dist2D(NewGrid.GetLocation(), Fixture.Car->GetActorLocation()) > 100.f);
	return true;
}

#endif
