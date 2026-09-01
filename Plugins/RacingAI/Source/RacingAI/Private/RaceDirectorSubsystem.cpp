#include "RaceDirectorSubsystem.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "RaceParticipantComponent.h"
#include "RacingAIComponent.h"
#include "RacingAIModule.h"
#include "RacingAIProfile.h"
#include "RacingSpline.h"

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

//------------------------------------------------------------------------------
// 등록
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

		// 레이스가 아직 시작되지 않았다면 그리드에서 대기시킵니다.
		if (bRaceStarted)
		{
			AI->BeginRacing();
		}
		else
		{
			AI->EnterWaiting();
		}
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

	if (AIComponents.Num() > 0)
	{
		AdvanceCursor %= AIComponents.Num();
	}
	else
	{
		AdvanceCursor = 0;
	}
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
// 트랙과 진행
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

void URaceDirectorSubsystem::HoldAtGrid()
{
	bRaceStarted = false;

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (AI)
		{
			AI->EnterWaiting();
		}
	}
}

void URaceDirectorSubsystem::StartRace()
{
	bRaceStarted = true;

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (AI)
		{
			AI->BeginRacing();
		}
	}

	UE_LOG(LogRacingAI, Log, TEXT("레이스 시작. 참가자 %d명 (AI %d대)"), Participants.Num(), AIComponents.Num());
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
		if (Participant)
		{
			Participant->RefreshProgress(*Track, DeltaTime);
		}
	}

	TArray<URaceParticipantComponent*> Ordered = GetParticipantsByPosition();

	for (int32 Index = 0; Index < Ordered.Num(); ++Index)
	{
		Ordered[Index]->Progress.Position = Index + 1;
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

FRacerAhead URaceDirectorSubsystem::FindRacerAhead(const URaceParticipantComponent* Self, float ScanDistance) const
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

	// 레이싱 라인
	const float Length = Track->GetLength();
	const float Step = 400.f;

	for (float D = 0.f; D < Length; D += Step)
	{
		DrawDebugLine(World, Track->GetLocationAtDistance(D), Track->GetLocationAtDistance(D + Step),
			FColor(60, 160, 160), false, -1.f, 0, 6.f);
	}

	for (const TObjectPtr<URacingAIComponent>& AI : AIComponents)
	{
		if (!AI || !AI->GetOwner())
		{
			continue;
		}

		const FVector Origin = AI->GetOwner()->GetActorLocation();

		// 배정된 차선 목표
		const FVector LaneTarget = Track->GetOffsetLocationAtDistance(
			AI->Progress.DistanceAlongSpline + 800.f, AI->TargetLaneOffset);

		DrawDebugLine(World, Origin, LaneTarget, FColor::Yellow, false, -1.f, 0, 4.f);
		DrawDebugSphere(World, LaneTarget, 40.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);

		const FString Text = FString::Printf(
			TEXT("P%d  %.0f/%.0f km/h  scale %.2f  lane %.0f  %s"),
			AI->Progress.Position,
			AI->Progress.ForwardSpeed * 0.036f,
			AI->TargetSpeed * 0.036f,
			AI->SpeedScale,
			AI->TargetLaneOffset,
			*UEnum::GetDisplayValueAsText(AI->State).ToString());

		DrawDebugString(World, FVector(0.f, 0.f, 220.f), Text, AI->GetOwner(), FColor::White, 0.f, true, 1.2f);
	}
}
