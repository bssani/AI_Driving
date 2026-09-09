#include "RacingLiveryApplierComponent.h"

#include "RacingAIModule.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

URacingLiveryApplierComponent::URacingLiveryApplierComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URacingLiveryApplierComponent::GatherMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UMeshComponent*> All;
	Owner->GetComponents(All);

	if (MeshComponentNames.Num() == 0)
	{
		OutMeshes = MoveTemp(All);

		return;
	}

	for (UMeshComponent* Mesh : All)
	{
		if (Mesh && MeshComponentNames.Contains(Mesh->GetFName()))
		{
			OutMeshes.Add(Mesh);
		}
	}
}

bool URacingLiveryApplierComponent::IsPaintSlot(const UMeshComponent& Mesh, int32 SlotIndex, FName SlotName) const
{
	// 직접 적어 준 자리가 가장 우선입니다.
	for (const FRacingLiverySlot& Target : PaintSlots)
	{
		if (Target.SlotIndex == SlotIndex
			&& (Target.MeshComponentName.IsNone() || Target.MeshComponentName == Mesh.GetFName()))
		{
			return true;
		}
	}

	if (!SlotName.IsNone() && PaintSlotNames.Contains(SlotName))
	{
		return true;
	}

	if (PaintMaterials.Num() == 0)
	{
		return false;
	}

	UMaterialInterface* Current = Mesh.GetMaterial(SlotIndex);
	if (!Current)
	{
		return false;
	}

	// 베이스 머티리얼로 비교합니다. 차량마다 색만 다른 인스턴스를 쓰고 있어도 한 식구로 잡힙니다.
	UMaterial* CurrentBase = Current->GetBaseMaterial();

	for (const TObjectPtr<UMaterialInterface>& Paint : PaintMaterials)
	{
		if (Paint && Paint->GetBaseMaterial() == CurrentBase)
		{
			return true;
		}
	}

	return false;
}

void URacingLiveryApplierComponent::ApplyLivery_Implementation(const FRacingLivery& Livery)
{
	TArray<UMeshComponent*> Meshes;
	GatherMeshes(Meshes);

	int32 PaintedSlots = 0;

	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
		const int32 SlotCount = Mesh->GetNumMaterials();

		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const FName SlotName = SlotNames.IsValidIndex(SlotIndex) ? SlotNames[SlotIndex] : NAME_None;

			if (!IsPaintSlot(*Mesh, SlotIndex, SlotName))
			{
				continue;
			}

			// 머티리얼 교체가 먼저입니다. 교체한 뒤에 그 위에서 MID를 만들어야
			// 색 파라미터가 새 머티리얼에 걸립니다.
			if (bReplaceMaterial && Livery.Material)
			{
				Mesh->SetMaterial(SlotIndex, Livery.Material);
			}

			if (!ColorParameterName.IsNone())
			{
				// 파라미터가 없는 머티리얼이면 아무 일도 일어나지 않습니다. 슬롯이 잘못
				// 잡혀도 조용히 넘어가므로 확인은 bLogApplied로 하세요.
				if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(SlotIndex))
				{
					Dynamic->SetVectorParameterValue(ColorParameterName, Livery.TintColor);
				}
			}

			++PaintedSlots;

			if (bLogApplied)
			{
				UE_LOG(LogRacingAI, Log, TEXT("%s: 도색 '%s'를 %s의 %d번 슬롯('%s')에 적용했습니다."),
					*GetNameSafe(GetOwner()), *Livery.Name.ToString(),
					*Mesh->GetName(), SlotIndex, *SlotName.ToString());
			}
		}
	}

	if (PaintedSlots == 0)
	{
		UE_LOG(LogRacingAI, Warning,
			TEXT("%s: 도색 '%s'를 적용할 슬롯을 찾지 못했습니다. PaintMaterials나 PaintSlotNames를 채우세요. "
				 "구성은 racing.DumpMaterialSlots로 확인할 수 있습니다."),
			*GetNameSafe(GetOwner()), *Livery.Name.ToString());
	}
}

FString URacingLiveryApplierComponent::DescribeMaterialSlots(AActor* Actor)
{
	if (!Actor)
	{
		return TEXT("액터가 없습니다.");
	}

	TArray<UMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("=== %s의 머티리얼 슬롯 ==="), *Actor->GetName()));

	for (const UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
		const int32 SlotCount = Mesh->GetNumMaterials();

		Lines.Add(FString::Printf(TEXT("  %s  (%s, 슬롯 %d개)"),
			*Mesh->GetName(), *Mesh->GetClass()->GetName(), SlotCount));

		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(SlotIndex);
			UMaterial* Base = Material ? Material->GetBaseMaterial() : nullptr;

			Lines.Add(FString::Printf(TEXT("    [%d] %-24s  %s   (베이스 %s)"),
				SlotIndex,
				SlotNames.IsValidIndex(SlotIndex) ? *SlotNames[SlotIndex].ToString() : TEXT("<이름 없음>"),
				*GetNameSafe(Material),
				*GetNameSafe(Base)));
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

//------------------------------------------------------------------------------
// 콘솔 명령
//------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorld GRacingDumpMaterialSlotsCommand(
	TEXT("racing.DumpMaterialSlots"),
	TEXT("플레이어 차량의 메시별 머티리얼 슬롯을 전부 찍습니다. 도색 대상을 정할 때 쓰세요."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;

			if (!Pawn)
			{
				UE_LOG(LogRacingAI, Warning, TEXT("플레이어 폰을 찾지 못했습니다."));

				return;
			}

			const FString Description = URacingLiveryApplierComponent::DescribeMaterialSlots(Pawn);

			UE_LOG(LogRacingAI, Log, TEXT("%s"), *Description);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Cyan, Description);
			}
		}));
