#include "RaceGridSpawner.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "RaceDirectorSubsystem.h"
#include "RaceParticipantComponent.h"
#include "RacingAIComponent.h"
#include "RacingAIModule.h"
#include "RacingSpline.h"
#include "TimerManager.h"

ARaceGridSpawner::ARaceGridSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARaceGridSpawner::BeginPlay()
{
	Super::BeginPlay();

	URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);
	if (!Director)
	{
		return;
	}

	Director->bDrawDebug = bDrawDebug;
	Director->bRubberBandingEnabled = bRubberBanding;
	Director->MaxAIUpdatesPerFrame = MaxAIUpdatesPerFrame;

	ARacingSpline* Track = Director->GetTrack();
	if (!Track)
	{
		UE_LOG(LogRacingAI, Error, TEXT("%s: 트랙(ARacingSpline)이 없어 그리드를 세울 수 없습니다."), *GetName());

		return;
	}

	// 그리드에 세운 뒤 카운트다운 동안 정지시켜 둡니다.
	SpawnGrid(*Track);
	Director->HoldAtGrid();

	if (bAutoTagPlayerPawn)
	{
		TryTagPlayerPawn();
	}

	if (bAutoStart)
	{
		if (CountdownSeconds <= 0.f)
		{
			StartRace();
		}
		else
		{
			GetWorldTimerManager().SetTimer(CountdownTimer, this, &ARaceGridSpawner::StartRace, CountdownSeconds, false);
		}
	}
}

void ARaceGridSpawner::SpawnGrid(ARacingSpline& Track)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Index = 0; Index < GridSlots.Num(); ++Index)
	{
		const FRaceGridSlot& Slot = GridSlots[Index];

		const TSubclassOf<APawn> ClassToSpawn = Slot.VehicleClassOverride ? Slot.VehicleClassOverride : VehicleClass;
		if (!ClassToSpawn)
		{
			UE_LOG(LogRacingAI, Warning, TEXT("%s: %d번 그리드에 차량 클래스가 없습니다."), *GetName(), Index);
			continue;
		}

		// 폴 포지션에서 뒤로 한 칸씩 물러나며 세웁니다.
		const float Distance = Track.WrapDistance(PoleDistance - Index * RowSpacing);

		const FVector Location = Track.GetOffsetLocationAtDistance(Distance, Slot.LaneOffset)
			+ FVector(0.f, 0.f, SpawnHeight);
		const FRotator Rotation = Track.GetDirectionAtDistance(Distance).Rotation();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;

		APawn* Vehicle = World->SpawnActor<APawn>(ClassToSpawn, Location, Rotation, Params);
		if (!Vehicle)
		{
			UE_LOG(LogRacingAI, Warning, TEXT("%s: %d번 그리드 차량 스폰에 실패했습니다."), *GetName(), Index);
			continue;
		}

		// 어댑터를 먼저 붙여야 합니다. 주행 컴포넌트는 추가되는 즉시 BeginPlay에서
		// 입력 구현체를 찾기 때문입니다.
		if (VehicleInputAdapterClass)
		{
			Vehicle->AddComponentByClass(VehicleInputAdapterClass, false, FTransform::Identity, false);
		}

		UActorComponent* Added = Vehicle->AddComponentByClass(
			URacingAIComponent::StaticClass(), false, FTransform::Identity, false);

		if (URacingAIComponent* AI = Cast<URacingAIComponent>(Added))
		{
			const FText Name = Slot.DriverName.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("AI %d"), Index + 1))
				: Slot.DriverName;

			AI->Configure(Slot.Profile, Slot.LaneOffset, Name);
		}

		SpawnedVehicles.Add(Vehicle);
	}

	UE_LOG(LogRacingAI, Log, TEXT("%s: AI 차량 %d대를 그리드에 세웠습니다."), *GetName(), SpawnedVehicles.Num());
}

void ARaceGridSpawner::TryTagPlayerPawn()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!PlayerPawn)
	{
		// 게임모드가 플레이어 폰을 만들기 전에 이 액터의 BeginPlay가 돌 수 있습니다.
		// 몇 번만 다시 시도하고 포기합니다.
		if (++PlayerTagAttempts < 25)
		{
			GetWorldTimerManager().SetTimer(PlayerTagTimer, this, &ARaceGridSpawner::TryTagPlayerPawn, 0.2f, false);
		}
		else
		{
			UE_LOG(LogRacingAI, Warning,
				TEXT("%s: 플레이어 폰을 찾지 못해 러버밴딩 기준이 없습니다. AI는 보정 없이 주행합니다."), *GetName());
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

		UE_LOG(LogRacingAI, Log, TEXT("%s: 플레이어 폰 %s을(를) 참가자로 등록했습니다."),
			*GetName(), *PlayerPawn->GetName());
	}
}

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

void ARaceGridSpawner::StartRace()
{
	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->StartRace();
	}
}
