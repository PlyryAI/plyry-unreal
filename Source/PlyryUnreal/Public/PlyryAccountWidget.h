#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlyryApiTypes.h"
#include "PlyryAccountWidget.generated.h"

class UButton;
class UBorder;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UPlyryAccountTheme;
class UPlyryApiClientSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlyryAccountTextEvent, const FText&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlyryAccountJsonEvent, const FString&, Json);

UCLASS(BlueprintType, Blueprintable)
class PLYRYUNREAL_API UPlyryAccountWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI")
    TObjectPtr<UPlyryAccountTheme> Theme;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI")
    bool bBuildDefaultLayout = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText HeaderText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText EmailHintText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText PasswordHintText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText ChallengeAnswerHintText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText SignInButtonText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText RequestChallengeButtonText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText SignUpButtonText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText RefreshButtonText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|UI|Text")
    FText SignOutButtonText;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|UI")
    FPlyryAccountTextEvent OnStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|UI")
    FPlyryAccountTextEvent OnSignupQuestionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|UI")
    FPlyryAccountJsonEvent OnSignupChallengeReceived;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|UI")
    FPlyryAccountJsonEvent OnAccountDataChanged;

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void SignIn();

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RequestSignupChallenge();

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RequestSignupChallengeForEmail(const FString& Email);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void SignUp();

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account", meta = (AdvancedDisplay = "RequestedScopes"))
    void SignInWithCredentials(const FString& Email, const FString& Password, const TArray<FString>& RequestedScopes);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account", meta = (AdvancedDisplay = "RequestedScopes"))
    void SignUpWithChallenge(const FString& Email, const FString& Password, const FString& Challenge, const FString& Answer, const TArray<FString>& RequestedScopes);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RefreshAccount();

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void SignOut();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleSignInClicked();

    UFUNCTION()
    void HandleChallengeClicked();

    UFUNCTION()
    void HandleSignUpClicked();

    UFUNCTION()
    void HandleRefreshClicked();

    UFUNCTION()
    void HandleSignOutClicked();

    UFUNCTION()
    void HandleRequestCompleted(const FPlyryApiResponse& Response);

private:
    UPROPERTY(Transient)
    TObjectPtr<UBorder> PanelBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> QuestionLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> AccountLabel;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> EmailInput;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> PasswordInput;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> AnswerInput;

    UPROPERTY(Transient)
    TObjectPtr<UPlyryApiClientSubsystem> Client;

    FString ActiveSignupChallenge;
    FString RequestIdPrefix;

    void BuildLocalLayout();
    UButton* AddButton(UVerticalBox* Parent, const FText& Text, const FName& Name);
    void ApplyTextStyle(UTextBlock* Text, bool bHeader) const;
    void ApplyInputStyle(UEditableTextBox* Input) const;
    void SetStatus(const FText& Text);
    FText ReadInput(const UEditableTextBox* Input) const;
    void BindClient();
    FString MakeRequestId(const FString& Suffix) const;
};
