// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GridMaterialRule.generated.h"

/**
 * @struct FGridMaterialRule
 * @brief A single rule that overrides grid cell properties based on UE5 Physical Material.
 *
 * During terrain scanning, the LineTrace hit result provides a UPhysicalMaterial reference.
 * Each rule's PhysicalMaterial is compared against the hit; first match in the array wins.
 *
 * When matched, the rule can override MoveCost, SpeedMultiplier, MaterialTag,
 * and optionally force the cell to be unwalkable.
 */
USTRUCT(BlueprintType)
struct U3D_SIMPLE_API FGridMaterialRule
{
	GENERATED_BODY()

	// ─────────────────────────────────────────────────────────────────────────
	// Match condition
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Physical Material to match against the terrain surface.
	 * Assign the same UPhysicalMaterial that is set on your landscape/mesh materials.
	 * This is the sole match condition — the rule fires when the traced surface has this PhysMat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule")
	TSoftObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	// ─────────────────────────────────────────────────────────────────────────
	// Override values
	// ─────────────────────────────────────────────────────────────────────────

	/** If true, this rule forces the cell's walkability to the value of bForceWalkable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule")
	bool bOverrideWalkable = false;

	/** Forced walkability value when bOverrideWalkable is true. false = impassable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule",
		meta = (EditCondition = "bOverrideWalkable"))
	bool bForceWalkable = false;

	/** Movement cost override. Default 1.0 = normal cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule",
		meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MoveCost = 1.f;

	/** Speed multiplier override. Default 1.0 = normal speed. Values < 1.0 slow the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule",
		meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SpeedMultiplier = 1.f;

	/** Gameplay tag to assign to the cell when this rule matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule")
	FGameplayTag MaterialTag;

	// ─────────────────────────────────────────────────────────────────────────
	// Display
	// ─────────────────────────────────────────────────────────────────────────

	/** Human-readable label for this rule (editor display only, no runtime effect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|MaterialRule")
	FString RuleName;
};
