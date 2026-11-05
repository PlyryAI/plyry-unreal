#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateTypes.h"
#include "PlyryAccountTheme.generated.h"

UCLASS(BlueprintType)
class PLYRYUNREAL_API UPlyryAccountTheme : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FLinearColor PanelColor = FLinearColor(0.035f, 0.045f, 0.065f, 0.98f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FLinearColor SurfaceColor = FLinearColor(0.075f, 0.095f, 0.13f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FLinearColor AccentColor = FLinearColor(0.24f, 0.58f, 0.92f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FLinearColor TextColor = FLinearColor(0.92f, 0.94f, 0.97f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FLinearColor MutedColor = FLinearColor(0.62f, 0.68f, 0.76f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FSlateFontInfo HeaderFont;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FSlateFontInfo BodyFont;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    bool bOverrideButtonStyle = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FButtonStyle ButtonStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    bool bOverrideInputStyle = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FEditableTextBoxStyle InputStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI")
    FMargin PanelPadding = FMargin(24.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plyry|UI", meta = (ClampMin = "260.0"))
    float PanelWidth = 420.0f;
};
