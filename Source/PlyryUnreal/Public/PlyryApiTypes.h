#pragma once

#include "CoreMinimal.h"
#include "PlyryApiTypes.generated.h"

UENUM(BlueprintType)
enum class EPlyryHttpVerb : uint8
{
    Get,
    Post,
    Put,
    Patch,
    Delete
};

USTRUCT(BlueprintType)
struct PLYRYUNREAL_API FPlyryApiResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    FString RequestId;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    int32 StatusCode = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    FString Error;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    FString Body;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    FString ContentType;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    TMap<FString, FString> Headers;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|API")
    TArray<uint8> Bytes;
};

USTRUCT(BlueprintType)
struct PLYRYUNREAL_API FPlyryFormField
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    FString Value;
};

USTRUCT(BlueprintType)
struct PLYRYUNREAL_API FPlyryFormFile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    FString FieldName = TEXT("files[]");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    FString FileName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    FString MimeType = TEXT("application/octet-stream");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plyry|API")
    TArray<uint8> Data;
};

USTRUCT(BlueprintType)
struct PLYRYUNREAL_API FPlyrySessionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|Account")
    bool bAuthenticated = false;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|Account")
    FString UserJson;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|Account")
    TArray<FString> Scopes;

    UPROPERTY(BlueprintReadOnly, Category = "Plyry|Account")
    FDateTime AccessTokenExpiresAt;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlyryApiResponseEvent, const FPlyryApiResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlyrySessionChangedEvent, const FPlyrySessionSnapshot&, Session);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlyryCredentialWarningEvent, const FString&, Message);

/**
 * Optional refresh-token persistence supplied by the host game.
 * The default client does not persist secrets. Implement these events with a
 * platform secure-storage facility before assigning this object.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew)
class PLYRYUNREAL_API UPlyryCredentialStore : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, Category = "Plyry|Security")
    FString LoadRefreshToken() const;
    virtual FString LoadRefreshToken_Implementation() const;

    UFUNCTION(BlueprintNativeEvent, Category = "Plyry|Security")
    void SaveRefreshToken(const FString& Token);
    virtual void SaveRefreshToken_Implementation(const FString& Token);

    UFUNCTION(BlueprintNativeEvent, Category = "Plyry|Security")
    void ClearRefreshToken();
    virtual void ClearRefreshToken_Implementation();
};
