// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GridMaterialRule.h"
#include "GridTypes.h"
#include "GridMaterialRuleDataAsset.generated.h"

/**
 * @class UGridMaterialRuleDataAsset
 * @brief Material rule configuration asset — holds an ordered list of FGridMaterialRule entries.
 *
 * During terrain scanning, each cell's hit material is tested against the rules in array order.
 * The first matching rule wins and its overrides are applied to the FGridCell.
 *
 * Create instances via Content Browser: Right-click → Miscellaneous → Data Asset → GridMaterialRuleDataAsset.
 */
UCLASS(BlueprintType)
class U3D_SIMPLE_API UGridMaterialRuleDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	/**
	 * @brief Ordered list of material rules. First match wins.
	 * @note  Rules are evaluated top-to-bottom; place higher-priority rules earlier in the array.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule",
		meta = (TitleProperty = "RuleName"))
	TArray<FGridMaterialRule> Rules;

	/**
	 * @brief Attempts to match a Physical Material against the rule table and apply overrides to a cell.
	 *
	 * @param HitPhysMat  The UPhysicalMaterial from the terrain LineTrace hit result.
	 * @param InOutCell   [in/out] The grid cell whose properties will be modified on match.
	 * @return true if a rule matched and was applied, false if no rule matched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|MaterialRule")
	bool TryMatchMaterial(UPhysicalMaterial* HitPhysMat, UPARAM(ref) FGridCell& InOutCell) const;
};
