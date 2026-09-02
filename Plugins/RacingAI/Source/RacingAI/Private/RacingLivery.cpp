#include "RacingLivery.h"

bool URacingLiverySet::FindLivery(FName InName, FRacingLivery& OutLivery) const
{
	if (InName.IsNone())
	{
		return false;
	}

	for (const FRacingLivery& Livery : Liveries)
	{
		if (Livery.Name == InName)
		{
			OutLivery = Livery;

			return true;
		}
	}

	return false;
}

bool URacingLiverySet::PickRandomLivery(int32 Seed, const TArray<FName>& ExcludeNames, FRacingLivery& OutLivery) const
{
	if (Liveries.Num() == 0)
	{
		return false;
	}

	TArray<int32> Candidates;
	Candidates.Reserve(Liveries.Num());

	for (int32 Index = 0; Index < Liveries.Num(); ++Index)
	{
		if (!ExcludeNames.Contains(Liveries[Index].Name))
		{
			Candidates.Add(Index);
		}
	}

	// 후보가 동나면 중복을 허용합니다. 도색 수보다 차가 많은 경우가 있고,
	// 그때 색을 못 고르는 것보다 겹치는 편이 낫습니다.
	if (Candidates.Num() == 0)
	{
		for (int32 Index = 0; Index < Liveries.Num(); ++Index)
		{
			Candidates.Add(Index);
		}
	}

	FRandomStream Stream(Seed);
	OutLivery = Liveries[Candidates[Stream.RandRange(0, Candidates.Num() - 1)]];

	return true;
}
