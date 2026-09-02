#include "RaceGridSpawner.h"

#include "Components/InputComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "RaceDirectorSubsystem.h"
#include "RaceParticipantComponent.h"
#include "RacingAIComponent.h"
#include "RacingAIModule.h"
#include "RacingAIProfile.h"
#include "RacingSpline.h"
#include "TimerManager.h"

ARaceGridSpawner::ARaceGridSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	StartRaceKey = EKeys::Enter;
	ResetRaceKey = EKeys::Delete;
	RestartRaceKey = EKeys::BackSpace;
}

void ARaceGridSpawner::BeginPlay()
{
	Super::BeginPlay();

	BuildGrid();

	if (bEnableControlKeys)
	{
		SetupControlKeys();
	}
}

void ARaceGridSpawner::SetupControlKeys()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);

	if (!PC)
	{
		// 게임모드가 컨트롤러를 만들기 전에 BeginPlay가 돌 수 있습니다.
		if (++ControlKeyAttempts < 25)
		{
			GetWorldTimerManager().SetTimer(ControlKeyTimer, this, &ARaceGridSpawner::SetupControlKeys, 0.2f, false);
		}

		return;
	}

	// 이 액터에 입력을 붙입니다. 차량 폰이 아니므로 출발 전 플레이어 잠금의
	// 영향을 받지 않습니다. 잠금은 폰의 입력 컴포넌트만 스택에서 빼기 때문입니다.
	EnableInput(PC);

	if (!InputComponent)
	{
		return;
	}

	if (StartRaceKey.IsValid())
	{
		InputComponent->BindKey(StartRaceKey, IE_Pressed, this, &ARaceGridSpawner::StartRace);
	}

	if (ResetRaceKey.IsValid())
	{
		InputComponent->BindKey(ResetRaceKey, IE_Pressed, this, &ARaceGridSpawner::ResetRace);
	}

	if (RestartRaceKey.IsValid())
	{
		InputComponent->BindKey(RestartRaceKey, IE_Pressed, this, &ARaceGridSpawner::RestartRace);
	}

	const FString Hint = FString::Printf(TEXT("[%s] 출발  [%s] 리셋  [%s] 재시작"),
		*StartRaceKey.GetDisplayName().ToString(),
		*ResetRaceKey.GetDisplayName().ToString(),
		*RestartRaceKey.GetDisplayName().ToString());

	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->ControlHint = Hint;
	}

	UE_LOG(LogRacingAI, Log, TEXT("%s: 조작 키 준비됨. %s"), *GetName(), *Hint);
}

void ARaceGridSpawner::StartRace()
{
	URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);
	if (!Director)
	{
		return;
	}

	// 이미 달리는 중이면 무시합니다. 다시 세우려면 Restart를 쓰세요.
	const ERaceState State = Director->GetRaceState();

	if (State == ERaceState::Racing || State == ERaceState::Countdown)
	{
		return;
	}

	Director->StartCountdown(CountdownSeconds);
}

void ARaceGridSpawner::ResetRace()
{
	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->ResetRace();
	}
}

void ARaceGridSpawner::RestartRace()
{
	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->RestartRace(CountdownSeconds);
	}
}

//------------------------------------------------------------------------------
// 그리드 구성
//------------------------------------------------------------------------------

void ARaceGridSpawner::ComputeGridDistance(const ARacingSpline& Track, int32 GridIndex, float& OutDistance, int32& OutInitialLap) const
{
	const float Length = FMath::Max(1.f, Track.GetLength());

	// Pole Distance는 결승선을 기준으로 한 상대 거리입니다. 음수면 결승선 뒤,
	// 즉 출발해서 한 바퀴를 돌아야 결승선에 닿는 정상적인 그리드 배치가 됩니다.
	const float Raw = PoleDistance - GridIndex * RowSpacing;

	OutDistance = Track.GetSplineDistanceFromFinishOffset(Raw);

	// 결승선 뒤쪽으로 감긴 자리는 아직 0랩을 시작하지 않은 상태입니다.
	// 이 값을 -1로 주지 않으면 누적 거리가 한 바퀴만큼 부풀어, 뒷줄 차량이
	// 출발도 하기 전에 1위로 표시되고 러버밴딩도 거꾸로 걸립니다.
	OutInitialLap = Track.IsClosed() ? FMath::FloorToInt(Raw / Length) : 0;
}

void ARaceGridSpawner::BuildGrid()
{
	if (bGridBuilt)
	{
		return;
	}

	URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);
	if (!Director)
	{
		return;
	}

	ARacingSpline* Track = Director->GetTrack();
	if (!Track)
	{
		UE_LOG(LogRacingAI, Error, TEXT("%s: 트랙(ARacingSpline)이 없어 그리드를 세울 수 없습니다."), *GetName());

		return;
	}

	bGridBuilt = true;

	Director->ClearGridPlacements();
	Director->SetTotalLaps(TotalLaps);

	Director->bDrawDebug = bDrawDebug;
	Director->bRubberBandingEnabled = bRubberBanding;
	Director->MaxAIUpdatesPerFrame = MaxAIUpdatesPerFrame;
	Director->bEndRaceWhenPlayerFinishes = bEndRaceWhenPlayerFinishes;
	Director->RaceTimeLimitSeconds = RaceTimeLimitSeconds;

	UsedLiveryNames.Reset();

	const int32 Seed = RandomSeed != 0 ? RandomSeed : FMath::Rand();
	FRandomStream Stream(Seed);

	// 슬롯을 어느 그리드 자리에 놓을지 결정합니다. 기본은 배열 순서 그대로입니다.
	TArray<int32> Order;
	Order.Reserve(GridSlots.Num());
	for (int32 Index = 0; Index < GridSlots.Num(); ++Index)
	{
		Order.Add(Index);
	}

	if (bShuffleGridOrder)
	{
		for (int32 Index = Order.Num() - 1; Index > 0; --Index)
		{
			Order.Swap(Index, Stream.RandRange(0, Index));
		}
	}

	for (int32 GridIndex = 0; GridIndex < Order.Num(); ++GridIndex)
	{
		PlaceSlot(*Track, GridSlots[Order[GridIndex]], GridIndex, Stream);
	}

	// Player 자리를 두지 않았다면, 있는 자리에서 참가자로만 등록합니다.
	if (!bHasPendingPlayerSlot && bAutoTagPlayerPawn)
	{
		TagPlayerInPlace();
	}

	Director->HoldAtGrid();

	UE_LOG(LogRacingAI, Log, TEXT("%s: 그리드 %d자리 구성 완료 (AI %d대, 시드 %d)"),
		*GetName(), GridSlots.Num(), SpawnedVehicles.Num(), Seed);

	if (bAutoStart)
	{
		Director->StartCountdown(CountdownSeconds);
	}
	else
	{
		UE_LOG(LogRacingAI, Log,
			TEXT("%s: 그리드 대기 중입니다. [%s] 키를 누르거나 Race Director의 Start Countdown을 "
				 "호출하면 출발합니다. 플레이어 조작은 출발 신호와 함께 풀립니다."),
			*GetName(), *StartRaceKey.GetDisplayName().ToString());
	}
}

void ARaceGridSpawner::PlaceSlot(ARacingSpline& Track, const FRaceGridSlot& Slot, int32 GridIndex, FRandomStream& Stream)
{
	float Distance = 0.f;
	int32 InitialLap = 0;
	ComputeGridDistance(Track, GridIndex, Distance, InitialLap);

	const FVector Location = Track.GetOffsetLocationAtDistance(Distance, Slot.LaneOffset) + FVector(0.f, 0.f, SpawnHeight);
	const FRotator Rotation = Track.GetDirectionAtDistance(Distance).Rotation();
	const FTransform Transform(Rotation, Location);

	if (Slot.Occupant == ERaceGridOccupant::Player)
	{
		PlacePlayer(Slot, Transform, GridIndex);

		return;
	}

	APawn* Vehicle = SpawnAIVehicle(Slot, Transform, GridIndex, Stream);
	if (!Vehicle)
	{
		return;
	}

	if (URaceParticipantComponent* Participant = Vehicle->FindComponentByClass<URaceParticipantComponent>())
	{
		if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
		{
			Director->RegisterGridPlacement(Participant, Transform, InitialLap, Slot.LaneOffset);
		}
	}
}

APawn* ARaceGridSpawner::SpawnAIVehicle(const FRaceGridSlot& Slot, const FTransform& Transform, int32 GridIndex, FRandomStream& Stream)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const TSubclassOf<APawn> ClassToSpawn = Slot.VehicleClassOverride ? Slot.VehicleClassOverride : VehicleClass;
	if (!ClassToSpawn)
	{
		UE_LOG(LogRacingAI, Warning, TEXT("%s: %d번 그리드에 차량 클래스가 없습니다."), *GetName(), GridIndex);

		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	APawn* Vehicle = World->SpawnActor<APawn>(ClassToSpawn, Transform.GetLocation(), Transform.Rotator(), Params);
	if (!Vehicle)
	{
		UE_LOG(LogRacingAI, Warning, TEXT("%s: %d번 그리드 차량 스폰에 실패했습니다."), *GetName(), GridIndex);

		return nullptr;
	}

	// 어댑터를 먼저 붙여야 합니다. 주행 컴포넌트는 추가되는 즉시 BeginPlay에서
	// 입력 구현체를 찾기 때문입니다.
	if (VehicleInputAdapterClass)
	{
		Vehicle->AddComponentByClass(VehicleInputAdapterClass, false, FTransform::Identity, false);
	}

	UActorComponent* Added = Vehicle->AddComponentByClass(
		URacingAIComponent::StaticClass(), false, FTransform::Identity, false);

	URacingAIComponent* AI = Cast<URacingAIComponent>(Added);
	if (AI)
	{
		URacingAIProfile* Profile = Slot.Profile;

		if (bRandomizeProfiles && ProfilePool.Num() > 0)
		{
			Profile = ProfilePool[Stream.RandRange(0, ProfilePool.Num() - 1)];
		}

		const FText Name = Slot.DriverName.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("AI %d"), GridIndex + 1))
			: Slot.DriverName;

		AI->Configure(Profile, Slot.LaneOffset, Name);

		ResolveAndApplyLivery(*Vehicle, *AI, Slot, Stream);
	}

	SpawnedVehicles.Add(Vehicle);

	return Vehicle;
}

//------------------------------------------------------------------------------
// 플레이어
//------------------------------------------------------------------------------

void ARaceGridSpawner::PlacePlayer(const FRaceGridSlot& Slot, const FTransform& Transform, int32 GridIndex)
{
	PendingPlayerSlot = Slot;
	PendingPlayerTransform = Transform;
	PendingPlayerGridIndex = GridIndex;
	bHasPendingPlayerSlot = true;
	PlayerPlacementAttempts = 0;

	RetryPlacePlayer();
}

void ARaceGridSpawner::RetryPlacePlayer()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!PlayerPawn)
	{
		// 게임모드가 플레이어 폰을 만들기 전에 이 액터의 BeginPlay가 돌 수 있습니다.
		if (++PlayerPlacementAttempts < 25)
		{
			GetWorldTimerManager().SetTimer(PlayerPlacementTimer, this, &ARaceGridSpawner::RetryPlacePlayer, 0.2f, false);
		}
		else
		{
			UE_LOG(LogRacingAI, Warning,
				TEXT("%s: 플레이어 폰을 찾지 못했습니다. 러버밴딩 기준이 없어 AI가 보정 없이 주행합니다."), *GetName());
		}

		return;
	}

	URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);
	ARacingSpline* Track = Director ? Director->GetTrack() : nullptr;
	if (!Director || !Track)
	{
		return;
	}

	URaceParticipantComponent* Participant = PlayerPawn->FindComponentByClass<URaceParticipantComponent>();

	if (!Participant)
	{
		UActorComponent* Added = PlayerPawn->AddComponentByClass(
			URaceParticipantComponent::StaticClass(), false, FTransform::Identity, false);

		Participant = Cast<URaceParticipantComponent>(Added);
	}

	if (!Participant)
	{
		return;
	}

	Participant->bIsPlayer = true;
	Participant->DisplayName = PendingPlayerSlot.DriverName.IsEmpty()
		? NSLOCTEXT("RacingAI", "PlayerDriverName", "Player")
		: PendingPlayerSlot.DriverName;

	// 사람이 타는 차는 게임모드가 이미 스폰했으므로, 그리드 자리로 옮기기만 합니다.
	PlayerPawn->SetActorTransform(PendingPlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(PlayerPawn->GetRootComponent()))
	{
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	FRandomStream Stream(RandomSeed != 0 ? RandomSeed + 7717 : FMath::Rand());
	ResolveAndApplyLivery(*PlayerPawn, *Participant, PendingPlayerSlot, Stream);

	float Distance = 0.f;
	int32 InitialLap = 0;
	ComputeGridDistance(*Track, PendingPlayerGridIndex, Distance, InitialLap);

	Director->RegisterGridPlacement(Participant, PendingPlayerTransform, InitialLap, PendingPlayerSlot.LaneOffset);

	UE_LOG(LogRacingAI, Log, TEXT("%s: 플레이어를 %d번 그리드에 배치했습니다."), *GetName(), PendingPlayerGridIndex + 1);
}

void ARaceGridSpawner::TagPlayerInPlace()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!PlayerPawn)
	{
		if (++PlayerPlacementAttempts < 25)
		{
			GetWorldTimerManager().SetTimer(PlayerPlacementTimer, this, &ARaceGridSpawner::TagPlayerInPlace, 0.2f, false);
		}

		return;
	}

	if (PlayerPawn->FindComponentByClass<URaceParticipantComponent>())
	{
		return;
	}

	UActorComponent* Added = PlayerPawn->AddComponentByClass(
		URaceParticipantComponent::StaticClass(), false, FTransform::Identity, false);

	if (URaceParticipantComponent* Participant = Cast<URaceParticipantComponent>(Added))
	{
		Participant->bIsPlayer = true;
		Participant->DisplayName = NSLOCTEXT("RacingAI", "PlayerDriverName", "Player");

		// 자리를 옮기지 않았으므로 현재 위치를 그대로 그리드 자리로 기록합니다.
		if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
		{
			Director->RegisterGridPlacement(Participant, PlayerPawn->GetActorTransform(), 0, 0.f);
		}
	}
}

//------------------------------------------------------------------------------
// 도색
//------------------------------------------------------------------------------

void ARaceGridSpawner::ResolveAndApplyLivery(AActor& Vehicle, URaceParticipantComponent& Participant, const FRaceGridSlot& Slot, FRandomStream& Stream)
{
	if (!LiverySet)
	{
		return;
	}

	FRacingLivery Livery;
	bool bFound = false;

	if (!Slot.LiveryName.IsNone())
	{
		bFound = LiverySet->FindLivery(Slot.LiveryName, Livery);

		if (!bFound)
		{
			UE_LOG(LogRacingAI, Warning, TEXT("%s: 도색 '%s'를 찾지 못했습니다."),
				*GetName(), *Slot.LiveryName.ToString());
		}
	}

	if (!bFound && bRandomizeLiveries)
	{
		const TArray<FName> Exclude = bAvoidDuplicateLiveries ? UsedLiveryNames : TArray<FName>();
		bFound = LiverySet->PickRandomLivery(static_cast<int32>(Stream.GetUnsignedInt()), Exclude, Livery);
	}

	if (!bFound)
	{
		return;
	}

	UsedLiveryNames.AddUnique(Livery.Name);

	Participant.LiveryName = Livery.Name;
	Participant.LiveryColor = Livery.TintColor;

	// 차량이 스스로 적용하겠다고 하면 그쪽에 맡깁니다. 차체가 여러 메시로 나뉘어
	// 있거나 데칼, 번호판까지 같이 바꿔야 하는 경우가 있기 때문입니다.
	if (Vehicle.GetClass()->ImplementsInterface(URacingVehicleLivery::StaticClass()))
	{
		IRacingVehicleLivery::Execute_ApplyLivery(&Vehicle, Livery);

		return;
	}

	TArray<UActorComponent*> Components;
	Vehicle.GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(URacingVehicleLivery::StaticClass()))
		{
			IRacingVehicleLivery::Execute_ApplyLivery(Component, Livery);

			return;
		}
	}

	// 아무도 구현하지 않았으면 지정된 슬롯에 직접 바릅니다.
	if (UMeshComponent* Mesh = FindLiveryMesh(Vehicle))
	{
		if (Livery.Material)
		{
			Mesh->SetMaterial(LiveryMaterialSlot, Livery.Material);
		}
	}
}

UMeshComponent* ARaceGridSpawner::FindLiveryMesh(AActor& Vehicle) const
{
	TArray<UMeshComponent*> Meshes;
	Vehicle.GetComponents(Meshes);

	if (!LiveryMeshComponentName.IsNone())
	{
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetFName() == LiveryMeshComponentName)
			{
				return Mesh;
			}
		}

		UE_LOG(LogRacingAI, Warning, TEXT("%s: 메시 컴포넌트 '%s'를 찾지 못했습니다. 루트 메시를 사용합니다."),
			*GetName(), *LiveryMeshComponentName.ToString());
	}

	// 이름을 지정하지 않았으면 추측해야 합니다. 이때 보이지 않는 메시를 제외하는 것이
	// 중요합니다. 차량 폰은 물리·애니메이션용 스켈레탈 메시를 루트로 두고 눈에 보이는
	// 차체는 별도 스태틱 메시로 붙이는 구성이 흔한데, 루트에 칠하면 겉보기에 아무
	// 변화가 없어 "도색이 안 먹는다"로 보입니다.
	TArray<UMeshComponent*> Visible;

	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->IsVisible() && Mesh->GetNumMaterials() > 0)
		{
			Visible.Add(Mesh);
		}
	}

	if (UMeshComponent* Root = Cast<UMeshComponent>(Vehicle.GetRootComponent()))
	{
		if (Visible.Contains(Root))
		{
			return Root;
		}
	}

	if (Visible.Num() == 1)
	{
		return Visible[0];
	}

	// 후보가 여럿이면 아무거나 고르지 않습니다. 잘못 칠하고 조용히 넘어가는 것보다
	// 무엇을 적어야 하는지 알려주는 편이 낫습니다.
	FString Candidates;

	for (const UMeshComponent* Mesh : Visible)
	{
		Candidates += FString::Printf(TEXT("\n    %s (슬롯 %d개)"), *Mesh->GetName(), Mesh->GetNumMaterials());
	}

	UE_LOG(LogRacingAI, Warning,
		TEXT("%s: 도색을 바를 메시를 특정할 수 없습니다. Livery Mesh Component Name 에 아래 중 하나를 적어 주세요.%s"),
		*GetNameSafe(&Vehicle), *Candidates);

	return nullptr;
}

//------------------------------------------------------------------------------

TArray<APawn*> ARaceGridSpawner::GetSpawnedVehicles() const
{
	TArray<APawn*> Result;
	Result.Reserve(SpawnedVehicles.Num());

	for (const TObjectPtr<APawn>& Vehicle : SpawnedVehicles)
	{
		Result.Add(Vehicle);
	}

	return Result;
}
