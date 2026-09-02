#include "RaceDirectorSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "RaceParticipantComponent.h"
#include "RacingAIComponent.h"
#include "RacingAIModule.h"
#include "RacingAIProfile.h"
#include "RacingSpline.h"
#include "TimerManager.h"

namespace
{
	/** 액터를 물리 상태까지 정리해 옮깁니다 */
	void TeleportAndSettle(AActor& Actor, const FTransform& Transform)
	{
		Actor.SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);

		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent()))
		{
			Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
}

URaceDirectorSubsystem* URaceDirectorSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	const UWorld* World = WorldContext->GetWorld();

	return World ? World->GetSubsystem<URaceDirectorSubsystem>() : nullptr;
}

TStatId URaceDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URaceDirectorSubsystem, STATGROUP_Tickables);
}

bool URaceDirectorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URaceDirectorSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}

	Super::Deinitialize();
}

//------------------------------------------------------------------------------
// 참가자
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::RegisterParticipant(URaceParticipantComponent* Participant)
{
	if (!Participant || Participants.Contains(Participant))
	{
		return;
	}

	Participants.Add(Participant);

	if (URacingAIComponent* AI = Cast<URacingAIComponent>(Participant))
	{
		AIComponents.Add(AI);

		if (RaceState == ERaceState::Racing)
		{
			AI->BeginRacing();
		}
		else
		{
			AI->EnterWaiting();
		}
	}
	else if (bLockPlayerUntilStart)
	{
		// 플레이어 폰은 게임모드가 늦게 만들기도 합니다. 등록 시점에 아직 출발 전이면
		// 그때 잠가야 카운트다운 도중에 등록된 차가 그냥 달려 나가지 않습니다.
		Participant->SetInputLocked(RaceState != ERaceState::Racing);
	}

	UE_LOG(LogRacingAI, Verbose, TEXT("참가자 등록: %s (AI=%s)"),
		*GetNameSafe(Participant->GetOwner()),
		Participant->IsA<URacingAIComponent>() ? TEXT("예") : TEXT("아니오"));
}

void URaceDirectorSubsystem::UnregisterParticipant(URaceParticipantComponent* Participant)
{
	Participants.Remove(Participant);

	if (URacingAIComponent* AI = Cast<URacingAIComponent>(Participant))
	{
		AIComponents.Remove(AI);
	}

	GridPlacements.RemoveAll([Participant](const FRaceGridPlacement& Placement)
	{
		return Placement.Participant.Get() == Participant;
	});

	AdvanceCursor = AIComponents.Num() > 0 ? AdvanceCursor % AIComponents.Num() : 0;
}

TArray<URaceParticipantComponent*> URaceDirectorSubsystem::GetParticipantsByPosition() const
{
	TArray<URaceParticipantComponent*> Result;
	Result.Reserve(Participants.Num());

	for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
	{
		if (Participant)
		{
			Result.Add(Participant);
		}
	}

	Result.Sort([](const URaceParticipantComponent& A, const URaceParticipantComponent& B)
	{
		// 완주자가 항상 앞에 옵니다. 결승선을 통과해 멈춰 선 차가 아직 달리는 차보다
		// 뒤로 밀려 보이면 결과 화면이 틀리게 됩니다.
		if (A.bFinished != B.bFinished)
		{
			return A.bFinished;
		}

		if (A.bFinished && B.bFinished)
		{
			return A.FinishPosition < B.FinishPosition;
		}

		return A.Progress.TotalDistance > B.Progress.TotalDistance;
	});

	return Result;
}

URaceParticipantComponent* URaceDirectorSubsystem::GetPlayerParticipant() const
{
	for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
	{
		if (Participant && Participant->bIsPlayer)
		{
			return Participant;
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// 그리드
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::RegisterGridPlacement(URaceParticipantComponent* Participant, const FTransform& Transform, int32 InitialLap, float LaneOffset)
{
	if (!Participant)
	{
		return;
	}

	GridPlacements.RemoveAll([Participant](const FRaceGridPlacement& Placement)
	{
		return Placement.Participant.Get() == Participant;
	});

	FRaceGridPlacement Placement;
	Placement.Participant = Participant;
	Placement.Transform = Transform;
	Placement.InitialLap = InitialLap;
	Placement.LaneOffset = LaneOffset;

	GridPlacements.Add(Placement);

	Participant->ResetProgress(InitialLap);
}

void URaceDirectorSubsystem::ClearGridPlacements()
{
	GridPlacements.Reset();
}

//------------------------------------------------------------------------------
// 레이스 수명주기
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::SetTotalLaps(int32 InTotalLaps)
{
	TotalLaps = FMath::Max(0, InTotalLaps);
}

float URaceDirectorSubsystem::GetRaceElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World || RaceStartTimeSeconds <= 0.f)
	{
		return 0.f;
	}

	return World->GetTimeSeconds() - RaceStartTimeSeconds;
}

void URaceDirectorSubsystem::HoldAtGrid()
{
	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (AI)
		{
			AI->EnterWaiting();
		}
	}

	// 사람도 함께 붙잡아 둡니다. AI만 잡아 두면 출발 신호 전에 플레이어 혼자
	// 먼저 나가 버려 카운트다운이 의미가 없어집니다.
	if (bLockPlayerUntilStart)
	{
		for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
		{
			if (Participant && !Participant->IsA<URacingAIComponent>())
			{
				Participant->SetInputLocked(true);
			}
		}
	}
}

void URaceDirectorSubsystem::StartCountdown(float Seconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	HoldAtGrid();

	RaceState = ERaceState::Countdown;
	CountdownRemaining = FMath::Max(0, FMath::CeilToInt(Seconds));

	OnCountdownStarted.Broadcast();
	OnCountdownTick.Broadcast(CountdownRemaining);

	if (CountdownRemaining <= 0)
	{
		StartRace();

		return;
	}

	World->GetTimerManager().SetTimer(CountdownTimer, this, &URaceDirectorSubsystem::TickCountdown, 1.f, true);
}

void URaceDirectorSubsystem::TickCountdown()
{
	--CountdownRemaining;

	OnCountdownTick.Broadcast(FMath::Max(0, CountdownRemaining));

	if (CountdownRemaining <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CountdownTimer);
		}

		StartRace();
	}
}

void URaceDirectorSubsystem::StartRace()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
		RaceStartTimeSeconds = World->GetTimeSeconds();
	}

	RaceState = ERaceState::Racing;
	FinishedCount = 0;

	// 출발과 동시에 사람의 조작을 풉니다.
	for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
	{
		if (Participant && !Participant->IsA<URacingAIComponent>())
		{
			Participant->SetInputLocked(false);
		}
	}

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (AI)
		{
			AI->BeginRacing();
		}
	}

	UE_LOG(LogRacingAI, Log, TEXT("레이스 시작. 참가자 %d명 (AI %d대), 목표 랩 %d"),
		Participants.Num(), AIComponents.Num(), TotalLaps);

	OnRaceStarted.Broadcast();
}

void URaceDirectorSubsystem::RestartRace(float CountdownSeconds)
{
	ResetRace();
	StartCountdown(CountdownSeconds);
}

void URaceDirectorSubsystem::AbortRace()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}

	RaceState = ERaceState::Aborted;
	HoldAtGrid();

	UE_LOG(LogRacingAI, Log, TEXT("레이스 중단"));
}

void URaceDirectorSubsystem::ConcludeRace()
{
	if (RaceState == ERaceState::Finished)
	{
		return;
	}

	RaceState = ERaceState::Finished;

	// 완주하지 못한 참가자에게도 진행도 순으로 등수를 줍니다. 결과 화면이
	// 빈칸으로 남지 않게 하고, 완주자와는 bDidNotFinish로 구분됩니다.
	int32 NextPosition = FinishedCount;

	for (URaceParticipantComponent* Participant : GetParticipantsByPosition())
	{
		if (Participant && !Participant->bFinished)
		{
			Participant->MarkDidNotFinish(++NextPosition);
		}
	}

	if (bStopAIAfterFinish)
	{
		for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
		{
			if (AI)
			{
				AI->EnterFinished();
			}
		}
	}

	UE_LOG(LogRacingAI, Log, TEXT("레이스 종료. 완주 %d명"), FinishedCount);

	OnRaceFinished.Broadcast();
}

void URaceDirectorSubsystem::ResetRace()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}

	// 기록해 둔 그리드 자리로 되돌립니다. 레벨을 다시 로드하지 않는 것이 요점입니다.
	for (const FRaceGridPlacement& Placement : GridPlacements)
	{
		URaceParticipantComponent* Participant = Placement.Participant.Get();
		if (!Participant)
		{
			continue;
		}

		if (AActor* Owner = Participant->GetOwner())
		{
			TeleportAndSettle(*Owner, Placement.Transform);
		}

		Participant->ResetProgress(Placement.InitialLap);

		if (URacingAIComponent* AI = Cast<URacingAIComponent>(Participant))
		{
			AI->ResetForNewRace(Placement.LaneOffset);
		}
	}

	RaceState = ERaceState::Idle;
	FinishedCount = 0;
	RaceStartTimeSeconds = 0.f;
	CountdownRemaining = 0;

	HoldAtGrid();

	UE_LOG(LogRacingAI, Log, TEXT("그리드 리셋. 참가자 %d명"), GridPlacements.Num());

	OnRaceReset.Broadcast();
}

//------------------------------------------------------------------------------
// 트랙
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::SetTrack(ARacingSpline* InTrack)
{
	Track = InTrack;
	bTrackResolved = true;
}

ARacingSpline* URaceDirectorSubsystem::GetTrack()
{
	ResolveTrackIfNeeded();

	return Track;
}

void URaceDirectorSubsystem::ResolveTrackIfNeeded()
{
	if (bTrackResolved || Track)
	{
		return;
	}

	// 월드 순회는 여기서 딱 한 번만 합니다. 원본 에셋은 차량마다 매 갱신
	// GetAllActorsOfClass를 호출했고, 그게 초당 수백 회의 월드 전수 순회가 됐습니다.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ARacingSpline> It(World); It; ++It)
		{
			Track = *It;
			break;
		}
	}

	bTrackResolved = true;

	if (!Track)
	{
		UE_LOG(LogRacingAI, Warning, TEXT("레벨에 ARacingSpline이 없습니다. AI가 주행하지 않습니다."));
	}
}

//------------------------------------------------------------------------------
// Tick
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ResolveTrackIfNeeded();

	if (!Track || Participants.Num() == 0)
	{
		return;
	}

	UpdateProgressAndPositions(DeltaTime);
	CheckForFinishers();

	// 제한 시간. 손님이 사고로 멈추면 완주 조건이 영영 충족되지 않으므로,
	// 이 검사가 없으면 세션이 끝나지 않습니다.
	if (RaceState == ERaceState::Racing && RaceTimeLimitSeconds > 0.f
		&& GetRaceElapsedSeconds() >= RaceTimeLimitSeconds)
	{
		UE_LOG(LogRacingAI, Log, TEXT("제한 시간 %.0f초 경과로 레이스를 종료합니다."), RaceTimeLimitSeconds);
		ConcludeRace();
	}
	UpdateRubberBanding();
	ArbitrateOvertaking();
	AdvanceAI(DeltaTime);

	if (bDrawDebug)
	{
		DrawDebug();
	}
}

void URaceDirectorSubsystem::UpdateProgressAndPositions(float DeltaTime)
{
	// 진행도 갱신은 전원 매 프레임입니다. 값이 싸고, 추월 판단이 항상 최신이어야 합니다.
	for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
	{
		if (!Participant)
		{
			continue;
		}

		const int32 LapDelta = Participant->RefreshProgress(*Track, DeltaTime);

		// 랩이 늘어난 순간에만 알립니다. 역주행으로 되돌아간 경우는 알리지 않습니다.
		if (LapDelta > 0 && RaceState == ERaceState::Racing)
		{
			OnLapCompleted.Broadcast(Participant, Participant->Progress.Lap);
		}
	}

	TArray<URaceParticipantComponent*> Ordered = GetParticipantsByPosition();

	for (int32 Index = 0; Index < Ordered.Num(); ++Index)
	{
		Ordered[Index]->Progress.Position = Index + 1;
	}
}

void URaceDirectorSubsystem::CheckForFinishers()
{
	if (RaceState != ERaceState::Racing || TotalLaps <= 0)
	{
		return;
	}

	bool bPlayerFinished = false;
	const bool bClosedCircuit = Track && Track->IsClosed();

	for (const TObjectPtr<URaceParticipantComponent>& Participant : Participants)
	{
		if (!Participant || Participant->bFinished)
		{
			continue;
		}

		// 닫힌 서킷은 정해진 랩을 채우면 완주입니다. 열린 코스(스프린트)는 랩이라는
		// 개념이 없으므로 결승선을 통과했는지만 봅니다.
		const bool bReachedFinish = bClosedCircuit
			? Participant->Progress.Lap >= TotalLaps
			: Participant->Progress.LapDistance >= 0.f;

		if (!bReachedFinish)
		{
			continue;
		}

		++FinishedCount;
		Participant->MarkFinished(FinishedCount, GetRaceElapsedSeconds());

		if (bStopAIAfterFinish)
		{
			if (URacingAIComponent* AI = Cast<URacingAIComponent>(Participant))
			{
				AI->EnterFinished();
			}
		}

		UE_LOG(LogRacingAI, Log, TEXT("완주: %s (%d위, %.2f초)"),
			*Participant->DisplayName.ToString(), FinishedCount, Participant->FinishTimeSeconds);

		OnRacerFinished.Broadcast(Participant, FinishedCount);

		if (Participant->bIsPlayer)
		{
			bPlayerFinished = true;
		}
	}

	const bool bAllFinished = FinishedCount >= Participants.Num();

	if (bAllFinished || (bPlayerFinished && bEndRaceWhenPlayerFinishes))
	{
		ConcludeRace();
	}
}

void URaceDirectorSubsystem::UpdateRubberBanding()
{
	const URaceParticipantComponent* Player = GetPlayerParticipant();

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (!AI)
		{
			continue;
		}

		const URacingAIProfile& P = AI->GetEffectiveProfile();

		if (!bRubberBandingEnabled || !Player || P.CatchUpStrength <= KINDA_SMALL_NUMBER)
		{
			AI->SpeedScale = 1.f;
			continue;
		}

		// 양수면 AI가 플레이어보다 뒤처져 있다는 뜻입니다.
		const float Gap = Player->Progress.TotalDistance - AI->Progress.TotalDistance;
		const float Magnitude = FMath::Abs(Gap);

		if (Magnitude <= P.NeutralGap)
		{
			// 접전 구간은 손대지 않습니다. 여기서 보정하면 플레이어가 조작감의 이상함을 느낍니다.
			AI->SpeedScale = 1.f;
			continue;
		}

		const float Span = FMath::Max(1.f, P.FullEffectGap - P.NeutralGap);
		const float T = FMath::Clamp((Magnitude - P.NeutralGap) / Span, 0.f, 1.f) * P.CatchUpStrength;

		AI->SpeedScale = Gap > 0.f
			? 1.f + T * P.MaxSpeedUp
			: 1.f - T * P.MaxSlowDown;
	}
}

FRacerAhead URaceDirectorSubsystem::FindRacerAhead(const URaceParticipantComponent* Self, float ScanDistance, float MaxLateralSeparation) const
{
	FRacerAhead Result;

	if (!Self || !Track)
	{
		return Result;
	}

	// 옆에 나란히 선 상태를 "앞"으로 오인하지 않도록 최소 간격을 둡니다.
	constexpr float MinMeaningfulGap = 50.f;

	float BestGap = TNumericLimits<float>::Max();

	for (const TObjectPtr<URaceParticipantComponent>& Other : Participants)
	{
		if (!Other || Other == Self)
		{
			continue;
		}

		const float Gap = Track->GetForwardDelta(Self->Progress.DistanceAlongSpline, Other->Progress.DistanceAlongSpline);

		if (Gap < MinMeaningfulGap || Gap > ScanDistance || Gap >= BestGap)
		{
			continue;
		}

		if (MaxLateralSeparation >= 0.f)
		{
			const float Separation = FMath::Abs(Other->Progress.LateralOffset - Self->Progress.LateralOffset);

			if (Separation >= MaxLateralSeparation)
			{
				continue;
			}
		}

		BestGap = Gap;

		Result.bValid = true;
		Result.Participant = Other;
		Result.Gap = Gap;
		Result.LateralOffset = Other->Progress.LateralOffset;
		Result.ForwardSpeed = Other->Progress.ForwardSpeed;
		Result.bIsPlayer = Other->bIsPlayer;
	}

	return Result;
}

void URaceDirectorSubsystem::ArbitrateOvertaking()
{
	// 앞선 차부터 결정하게 해서, 뒤차가 앞차의 선택을 보고 반대쪽을 고르도록 합니다.
	TArray<TObjectPtr<URacingAIComponent>> Ordered = AIComponents;
	Ordered.Sort([](const URacingAIComponent& A, const URacingAIComponent& B)
	{
		return A.Progress.TotalDistance > B.Progress.TotalDistance;
	});

	// 같은 대상을 향해 이미 어느 쪽이 점유됐는지 기록합니다.
	TMap<const URaceParticipantComponent*, float> ClaimedSides;

	const float HalfWidth = Track->TrackHalfWidth;

	for (const TObjectPtr<URacingAIComponent>& AI : Ordered)
	{
		if (!AI)
		{
			continue;
		}

		const URacingAIProfile& P = AI->GetEffectiveProfile();

		const FRacerAhead Ahead = FindRacerAhead(AI, P.OvertakeScanDistance);
		AI->RacerAhead = Ahead;

		// 제동용 탐색 거리는 속도에 따라 늘어나야 합니다. 고정 30m로는 시속 100km에서
		// 정지 거리(수십~100m 이상)를 감당할 수 없어, 앞차를 본 순간 이미 늦습니다.
		// 트랙 곡률을 볼 때 쓰는 것과 같은 방식입니다.
		const float Speed = FMath::Max(0.f, AI->Progress.ForwardSpeed);
		const float StoppingDistance = (Speed * Speed) / (2.f * FMath::Max(1.f, P.BrakingDecel));
		const float BrakeScan = FMath::Max(P.OvertakeScanDistance, StoppingDistance * 1.3f + P.FollowGap);

		AI->BlockingRacer = FindRacerAhead(AI, BrakeScan, P.PassingLateralClearance);

		float Desired = AI->BaseLaneOffset;
		bool bPass = false;

		if (Ahead.bValid)
		{
			// 기준은 "방해가 없었다면 낼 속도"입니다. 현재 속도로 재면 앞차 때문에
			// 멈춘 순간 0 < 0 이 되어 추월 의사가 취소되고, 다시 굴러가면 켜지는 일이
			// 반복되어 앞차 뒤에서 머뭇거리게 됩니다.
			const float Reference = FMath::Max(AI->FreeTargetSpeed, AI->Progress.ForwardSpeed);
			bPass = Ahead.ForwardSpeed < Reference * P.OvertakeSpeedRatio;

			if (bPass)
			{
				// 이미 다른 차선만큼 벌어져 있으면 굳이 더 움직이지 않습니다.
				const float BaseSeparation = FMath::Abs(AI->BaseLaneOffset - Ahead.LateralOffset);

				if (BaseSeparation < P.PassingLateralClearance)
				{
					// 앞차가 있는 쪽의 반대편으로 나갑니다.
					float Side = Ahead.LateralOffset >= 0.f ? -1.f : 1.f;

					// 다른 AI가 이미 그쪽을 잡았으면 반대쪽으로 돌립니다.
					if (const float* Claimed = ClaimedSides.Find(Ahead.Participant.Get()))
					{
						if (FMath::IsNearlyEqual(*Claimed, Side))
						{
							Side = -Side;
						}
					}

					// 비켜서는 폭은 최소한 "다른 차선"으로 인정되는 만큼은 되어야 합니다.
					// 그보다 좁으면 옆에 붙은 채로 간격 유지에 걸려 추월이 끝나지 않습니다.
					const float Step = FMath::Max(P.OvertakeLateralOffset, P.PassingLateralClearance);

					Desired = Ahead.LateralOffset + Side * Step;
					ClaimedSides.Add(Ahead.Participant.Get(), Side);
				}
			}

			// 플레이어에게는 더 넓게 비켜 줍니다. VR에서 옆에서 받히면 경쟁이 아니라 멀미가 됩니다.
			if (Ahead.bIsPlayer && Ahead.Gap < P.PlayerYieldDistance)
			{
				const float AwaySign = (Desired - Ahead.LateralOffset) >= 0.f ? 1.f : -1.f;
				Desired += AwaySign * P.PlayerExtraClearance;
			}
		}

		AI->bIntendToPass = bPass;
		AI->TargetLaneOffset = FMath::Clamp(Desired, -HalfWidth, HalfWidth);
	}
}

void URaceDirectorSubsystem::AdvanceAI(float DeltaTime)
{
	const int32 Num = AIComponents.Num();
	if (Num == 0)
	{
		return;
	}

	int32 Overdue = 0;

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (AI)
		{
			AI->TimeSinceAdvance += DeltaTime;

			if (AI->TimeSinceAdvance >= MaxUpdateInterval)
			{
				++Overdue;
			}
		}
	}

	// 기본 예산은 프레임당 MaxAIUpdatesPerFrame 대이지만, 최소 갱신 주기를 넘긴 AI가
	// 그보다 많으면 그만큼 늘립니다. 대수가 적을 때는 항상 한 대씩 균일하게 돕니다.
	const int32 Budget = FMath::Clamp(FMath::Max(MaxAIUpdatesPerFrame, Overdue), 1, Num);

	for (int32 Step = 0; Step < Budget; ++Step)
	{
		URacingAIComponent* AI = AIComponents[AdvanceCursor];
		AdvanceCursor = (AdvanceCursor + 1) % Num;

		if (!AI)
		{
			continue;
		}

		AI->Advance(*Track, AI->TimeSinceAdvance);
		AI->TimeSinceAdvance = 0.f;
	}
}

//------------------------------------------------------------------------------
// 디버그
//------------------------------------------------------------------------------

void URaceDirectorSubsystem::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World || !Track)
	{
		return;
	}

	const float Length = Track->GetLength();
	const float Step = 400.f;

	for (float D = 0.f; D < Length; D += Step)
	{
		DrawDebugLine(World, Track->GetLocationAtDistance(D), Track->GetLocationAtDistance(D + Step),
			FColor(60, 160, 160), false, -1.f, 0, 6.f);
	}

	// 현재 레이스 상태를 화면 위에 띄웁니다. 대기 중인지 달리는 중인지 모르면
	// "왜 안 움직이지"로 헤매게 됩니다.
	if (GEngine)
	{
		FString Status;

		switch (RaceState)
		{
		case ERaceState::Idle:      Status = TEXT("대기 중"); break;
		case ERaceState::Countdown: Status = FString::Printf(TEXT("카운트다운 %d"), CountdownRemaining); break;
		case ERaceState::Racing:    Status = FString::Printf(TEXT("주행 중  %.0f초  랩 %d"), GetRaceElapsedSeconds(), TotalLaps); break;
		case ERaceState::Finished:  Status = TEXT("종료"); break;
		case ERaceState::Aborted:   Status = TEXT("중단됨"); break;
		}

		// 달리는 중에는 조작 안내가 방해되므로 대기·종료 상태에서만 붙입니다.
		const bool bShowHint = !ControlHint.IsEmpty()
			&& (RaceState == ERaceState::Idle || RaceState == ERaceState::Finished || RaceState == ERaceState::Aborted);

		GEngine->AddOnScreenDebugMessage(0x5AC1, 0.f, FColor::Yellow,
			bShowHint
				? FString::Printf(TEXT("[Racing AI] %s    %s"), *Status, *ControlHint)
				: FString::Printf(TEXT("[Racing AI] %s"), *Status));
	}

	// 결승선. 어디서 랩이 올라가고 어디서 멈추는지 눈으로 확인할 수 있어야 합니다.
	{
		const float FinishDistance = Track->FinishLineDistance;
		const FVector Center = Track->GetLocationAtDistance(FinishDistance) + FVector(0.f, 0.f, 20.f);
		const FVector Right = Track->GetRightAtDistance(FinishDistance) * Track->TrackHalfWidth;

		DrawDebugLine(World, Center - Right, Center + Right, FColor::White, false, -1.f, 0, 18.f);
		DrawDebugString(World, Center + FVector(0.f, 0.f, 260.f), TEXT("FINISH"), nullptr, FColor::White, 0.f, true, 1.6f);
	}

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (!AI || !AI->GetOwner())
		{
			continue;
		}

		const FVector Origin = AI->GetOwner()->GetActorLocation();

		const FVector LaneTarget = Track->GetOffsetLocationAtDistance(
			AI->Progress.DistanceAlongSpline + 800.f, AI->TargetLaneOffset);

		DrawDebugLine(World, Origin, LaneTarget, FColor::Yellow, false, -1.f, 0, 4.f);
		DrawDebugSphere(World, LaneTarget, 40.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);

		const FString Text = FString::Printf(
			TEXT("P%d L%d/%d  %.0f/%.0f km/h  scale %.2f  %s"),
			AI->Progress.Position,
			FMath::Max(0, AI->Progress.Lap),
			TotalLaps,
			AI->Progress.ForwardSpeed * 0.036f,
			AI->TargetSpeed * 0.036f,
			AI->SpeedScale,
			*UEnum::GetDisplayValueAsText(AI->State).ToString());

		DrawDebugString(World, FVector(0.f, 0.f, 220.f), Text, AI->GetOwner(), FColor::White, 0.f, true, 1.2f);
	}
}
