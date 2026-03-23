// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.

#include "GridMaterialRuleDataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

bool UGridMaterialRuleDataAsset::TryMatchMaterial(UPhysicalMaterial* HitPhysMat,
                                                   FGridCell& InOutCell) const
{
	if (!HitPhysMat) return false;

	for (const FGridMaterialRule& Rule : Rules)
	{
		// Skip rules with no Physical Material assigned
		if (Rule.PhysicalMaterial.IsNull()) continue;

		UPhysicalMaterial* RulePhysMat = Rule.PhysicalMaterial.LoadSynchronous();
		if (!RulePhysMat || HitPhysMat != RulePhysMat) continue;

		// Rule matched — apply overrides
		InOutCell.MoveCost        = Rule.MoveCost;
		InOutCell.SpeedMultiplier = Rule.SpeedMultiplier;
		InOutCell.MaterialTag     = Rule.MaterialTag;

		if (Rule.bOverrideWalkable)
		{
			InOutCell.bWalkable = Rule.bForceWalkable;
		}

		return true;
	}

	return false;
}
