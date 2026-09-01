//Copyright 2024 P.Kallisto 


#include "Vehicle/RTuneVehicleWheel.h"

URTuneVehicleWheel::URTuneVehicleWheel()
{
	ComponentTags.Add(FName("RTuneWheel"));
}

bool URTuneVehicleWheel::IsLeftWheel()
{
	return bRotateWheel;
}
