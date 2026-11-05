#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlyryApiTypes.h"
#include "PlyryApiClientSubsystem.generated.h"

class UPlyryCredentialStore;

UCLASS(Config = Game, DefaultConfig)
class PLYRYUNREAL_API UPlyryApiClientSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPlyryApiClientSubsystem();

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Plyry|API")
    FString ApiBaseUrl;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|API")
    FPlyryApiResponseEvent OnRequestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|Account")
    FPlyrySessionChangedEvent OnSessionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Plyry|Security")
    FPlyryCredentialWarningEvent OnCredentialWarning;

    UFUNCTION(BlueprintCallable, Category = "Plyry|API")
    bool SetApiBaseUrl(const FString& NewBaseUrl);

    UFUNCTION(BlueprintPure, Category = "Plyry|API")
    FString GetApiBaseUrl() const { return ApiBaseUrl; }

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void SetLinkedAppName(const FString& AppName);

    UFUNCTION(BlueprintPure, Category = "Plyry|Account")
    FString GetLinkedAppName() const { return LinkedAppName; }

    UFUNCTION(BlueprintCallable, Category = "Plyry|Security", meta = (DisplayName = "Set API key (trusted server only)"))
    bool SetApiKey(const FString& ApiKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Security")
    void ClearApiKey();

    UFUNCTION(BlueprintCallable, Category = "Plyry|Security")
    void SetCredentialStore(UPlyryCredentialStore* Store);

    UFUNCTION(BlueprintPure, Category = "Plyry|Account")
    FPlyrySessionSnapshot GetSessionSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RestoreSession(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RequestSignupChallenge(const FString& RequestId, const FString& Email);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account", meta = (AdvancedDisplay = "RequestedScopes"))
    void SignIn(const FString& RequestId, const FString& Email, const FString& Password, const TArray<FString>& RequestedScopes);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account", meta = (AdvancedDisplay = "RequestedScopes"))
    void SignUp(const FString& RequestId, const FString& Email, const FString& Password, const FString& Challenge, const FString& Answer, const TArray<FString>& RequestedScopes);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void RefreshSession(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void SignOut(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|API", meta = (AdvancedDisplay = "JsonBody, bAuthenticated, IdempotencyKey, ExtraHeaders"))
    bool RequestJson(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const FString& JsonBody, bool bAuthenticated, const FString& IdempotencyKey, const TMap<FString, FString>& ExtraHeaders);

    UFUNCTION(BlueprintCallable, Category = "Plyry|API", meta = (AdvancedDisplay = "bAuthenticated, IdempotencyKey"))
    bool RequestBytes(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const TArray<uint8>& Body, const FString& ContentType, bool bAuthenticated, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|API", meta = (AdvancedDisplay = "Fields, Files, IdempotencyKey"))
    bool RequestMultipart(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, bool bAuthenticated, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|API")
    void GetOpenApiSpecification(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|API")
    void GetModels(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Account")
    void GetMe(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Wallet")
    void GetWallet(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Wallet")
    void GetWalletLedger(const FString& RequestId, int32 Page);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Payments")
    void GetPaymentConfig(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void QuoteGeneration(const FString& RequestId, const FString& JsonBody);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void CreateGeneration(const FString& RequestId, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void ListGenerations(const FString& RequestId, const FString& QueryString);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void GetGeneration(const FString& RequestId, const FString& Hash);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void DownloadGenerationMedia(const FString& RequestId, const FString& Hash, bool bScene);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Generation")
    void AcknowledgeGenerationCache(const FString& RequestId, const FString& Hash, const FString& CacheToken, const FString& Sha256);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Wallet")
    void CreateWalletTopup(const FString& RequestId, const FString& JsonBody, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Wallet")
    void GetWalletTopupStatus(const FString& RequestId, const FString& RequestKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Wallet")
    void ReconcileWalletTopup(const FString& RequestId, const FString& JsonBody, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void GetPlant3DToRevitPrice(const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void CreatePlant3DToRevitJob(const FString& RequestId, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, const FString& IdempotencyKey);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void GetPlant3DToRevitJob(const FString& RequestId, const FString& Hash);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void SearchPlant3DToRevitButtons(const FString& RequestId, const FString& Hash, const FString& QueryString);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void RevisePlant3DToRevitMapping(const FString& RequestId, const FString& Hash, const FString& JsonBody);

    UFUNCTION(BlueprintCallable, Category = "Plyry|Conversion")
    void DownloadPlant3DToRevitMapping(const FString& RequestId, const FString& Hash);

private:
    using FRefreshCallback = TFunction<void(bool, FPlyryApiResponse)>;
    using FRequestCallback = TFunction<void(FPlyryApiResponse)>;

    UPROPERTY(Transient)
    TObjectPtr<UPlyryCredentialStore> CredentialStore;

    FString LinkedAppName;
    FString AccessToken;
    FString RefreshToken;
    FString ApiKey;
    FString UserJson;
    TArray<FString> GrantedScopes;
    FDateTime AccessTokenExpiresAt;
    int32 SessionRevision = 0;
    bool bRefreshInProgress = false;
    TArray<FRefreshCallback> RefreshWaiters;

    void SendHttp(const FString& RequestId, const FString& Path, const FString& Verb, const TArray<uint8>& Body, const FString& ContentType, bool bAuthenticated, const FString& IdempotencyKey, const TMap<FString, FString>& ExtraHeaders, bool bAllowRefreshRetry, int32 RetryCount, FRequestCallback Completion);
    void RefreshAccessToken(FRefreshCallback Completion);
    void CompleteRefresh(bool bSuccess, const FPlyryApiResponse& Response);
    bool ApplyTokenResponse(FPlyryApiResponse& Response);
    void ClearSession(bool bClearPersistentToken, bool bBroadcast);
    void EmitLocalFailure(const FString& RequestId, const FString& Message);
    void BroadcastSessionChanged() const;
    bool BuildUrl(const FString& Path, FString& OutUrl) const;
    bool ValidateBaseUrl(const FString& Url) const;
    bool ValidateRequestPath(const FString& Path) const;
    bool ValidateExtraHeaders(const TMap<FString, FString>& Headers, FString& OutError) const;
    bool IsSensitiveAuthPath(const FString& Path) const;
    static bool SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutJson);
    static bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject);
    static FString SafeAuthBody(const FString& Json);
    static FString VerbToString(EPlyryHttpVerb Verb);
    static FString QueryPath(const FString& Path, const FString& QueryString);
};
