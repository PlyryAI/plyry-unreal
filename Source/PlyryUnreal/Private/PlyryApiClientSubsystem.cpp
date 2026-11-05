#include "PlyryApiClientSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace Plyry
{
    static void AppendUtf8(TArray<uint8>& Output, const FString& Text)
    {
        FTCHARToUTF8 Converted(*Text);
        Output.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    }

    static TArray<uint8> MakeJsonBody(const FString& Json)
    {
        FTCHARToUTF8 Converted(*Json);
        TArray<uint8> Body;
        Body.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
        return Body;
    }

    static bool IsHeaderSafe(const FString& Value)
    {
        for (const TCHAR Character : Value)
        {
            if (Character < 0x20 || Character == 0x7f) { return false; }
        }
        return true;
    }

    static bool IsJsonContentType(const FString& ContentType)
    {
        return ContentType.Contains(TEXT("json"), ESearchCase::IgnoreCase) ||
            ContentType.Contains(TEXT("text/"), ESearchCase::IgnoreCase);
    }

    static FString SafeHeaderFilename(const FString& InName)
    {
        FString Name = FPaths::GetCleanFilename(InName);
        if (Name.IsEmpty())
        {
            Name = TEXT("upload.bin");
        }
        for (TCHAR& Character : Name)
        {
            if (Character < 0x20 || Character == 0x7f || Character == TEXT('"') || Character == TEXT(';'))
            {
                Character = TEXT('_');
            }
        }
        return Name;
    }

    static bool IsSafeFormName(const FString& Name)
    {
        if (Name.IsEmpty())
        {
            return false;
        }
        for (const TCHAR Character : Name)
        {
            const bool bAllowed =
                (Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('_') || Character == TEXT('-') ||
                Character == TEXT('[') || Character == TEXT(']');
            if (!bAllowed)
            {
                return false;
            }
        }
        return true;
    }

    static bool IsSafeRouteSegment(const FString& Segment)
    {
        if (Segment.IsEmpty() || Segment.Len() > 128) { return false; }
        for (const TCHAR Character : Segment)
        {
            const bool bAllowed =
                (Character >= 'a' && Character <= 'z') ||
                (Character >= 'A' && Character <= 'Z') ||
                (Character >= '0' && Character <= '9') ||
                Character == '-' || Character == '_';
            if (!bAllowed) { return false; }
        }
        return true;
    }

    static FString ExtractErrorMessage(const FString& Body)
    {
        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
        {
            FString Message;
            if (Object->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
            {
                return Message;
            }
            if (Object->TryGetStringField(TEXT("error"), Message) && !Message.IsEmpty())
            {
                return Message;
            }
        }
        return FString();
    }

    static void RemoveSensitiveTokenFields(const TSharedPtr<FJsonObject>& Object)
    {
        if (!Object.IsValid()) { return; }

        // Keep this on FJsonObject's public API so it works across UE5's
        // FString and shared-string JSON key implementations.
        static const TCHAR* SensitiveFieldNames[] = {
            TEXT("access_token"), TEXT("refresh_token"),
            TEXT("accessToken"), TEXT("refreshToken"),
            TEXT("Access_Token"), TEXT("Refresh_Token"),
            TEXT("AccessToken"), TEXT("RefreshToken"),
            TEXT("access-token"), TEXT("refresh-token"),
            TEXT("Access-Token"), TEXT("Refresh-Token"),
            TEXT("ACCESS_TOKEN"), TEXT("REFRESH_TOKEN"),
            TEXT("ACCESSTOKEN"), TEXT("REFRESHTOKEN"),
            TEXT("ACCESS-TOKEN"), TEXT("REFRESH-TOKEN")
        };
        for (const TCHAR* FieldName : SensitiveFieldNames)
        {
            Object->RemoveField(FieldName);
        }
    }
}

FString UPlyryCredentialStore::LoadRefreshToken_Implementation() const { return FString(); }
void UPlyryCredentialStore::SaveRefreshToken_Implementation(const FString&) {}
void UPlyryCredentialStore::ClearRefreshToken_Implementation() {}

UPlyryApiClientSubsystem::UPlyryApiClientSubsystem()
    : ApiBaseUrl(TEXT("https://plyry.com/v1"))
{
    LinkedAppName = TEXT("Plyry Unreal");
}

bool UPlyryApiClientSubsystem::SetApiBaseUrl(const FString& NewBaseUrl)
{
    if (!ValidateBaseUrl(NewBaseUrl))
    {
        return false;
    }
    FString NormalizedBaseUrl = NewBaseUrl.TrimStartAndEnd();
    while (NormalizedBaseUrl.EndsWith(TEXT("/")))
    {
        NormalizedBaseUrl.LeftChopInline(1);
    }
    const bool bRootChanged = !ApiBaseUrl.Equals(NormalizedBaseUrl, ESearchCase::CaseSensitive);
    if (bRootChanged)
    {
        ++SessionRevision;
    }
    ApiBaseUrl = MoveTemp(NormalizedBaseUrl);
    if (bRootChanged)
    {
        // Never forward credentials issued by one API root to a different root.
        ClearApiKey();
        ClearSession(true, true);
    }
    return true;
}

void UPlyryApiClientSubsystem::SetLinkedAppName(const FString& AppName)
{
    LinkedAppName = AppName.TrimStartAndEnd();
    if (LinkedAppName.IsEmpty())
    {
        LinkedAppName = TEXT("Plyry Unreal");
    }
}

bool UPlyryApiClientSubsystem::SetApiKey(const FString& InApiKey)
{
    const FString Trimmed = InApiKey.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || !Plyry::IsHeaderSafe(Trimmed))
    {
        return false;
    }
    ApiKey = Trimmed;
    return true;
}

void UPlyryApiClientSubsystem::ClearApiKey()
{
    ApiKey.Reset();
}

void UPlyryApiClientSubsystem::SetCredentialStore(UPlyryCredentialStore* Store)
{
    CredentialStore = Store;
    if (CredentialStore == nullptr)
    {
        OnCredentialWarning.Broadcast(TEXT("No refresh-token store is configured. Sign-in will last for this application session only."));
    }
}

FPlyrySessionSnapshot UPlyryApiClientSubsystem::GetSessionSnapshot() const
{
    FPlyrySessionSnapshot Snapshot;
    Snapshot.bAuthenticated = !AccessToken.IsEmpty() && FDateTime::UtcNow() < AccessTokenExpiresAt;
    Snapshot.UserJson = UserJson;
    Snapshot.Scopes = GrantedScopes;
    Snapshot.AccessTokenExpiresAt = AccessTokenExpiresAt;
    return Snapshot;
}

void UPlyryApiClientSubsystem::RestoreSession(const FString& RequestId)
{
    if (!AccessToken.IsEmpty() && FDateTime::UtcNow() < AccessTokenExpiresAt)
    {
        FPlyryApiResponse Response;
        Response.RequestId = RequestId;
        Response.bSuccess = true;
        Response.StatusCode = 200;
        Response.Body = UserJson;
        OnRequestCompleted.Broadcast(Response);
        return;
    }
    if (RefreshToken.IsEmpty() && CredentialStore != nullptr)
    {
        RefreshToken = UPlyryCredentialStore::Execute_LoadRefreshToken(CredentialStore).TrimStartAndEnd();
    }
    if (RefreshToken.IsEmpty())
    {
        FPlyryApiResponse Response;
        Response.RequestId = RequestId;
        Response.bSuccess = true;
        Response.StatusCode = 204;
        Response.Error = TEXT("No saved Plyry session is available.");
        OnRequestCompleted.Broadcast(Response);
        return;
    }
    RefreshSession(RequestId);
}

void UPlyryApiClientSubsystem::RequestSignupChallenge(const FString& RequestId, const FString& Email)
{
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("email"), Email);
    FString Json;
    SerializeObject(Payload, Json);
    const TArray<uint8> Body = Plyry::MakeJsonBody(Json);
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(RequestId, TEXT("/auth/signup/challenges"), TEXT("POST"), Body, TEXT("application/json; charset=utf-8"), false, TEXT(""), TMap<FString, FString>(), false, 0,
        [WeakThis](FPlyryApiResponse Response)
        {
            if (WeakThis.IsValid())
            {
                Response.Body = UPlyryApiClientSubsystem::SafeAuthBody(Response.Body);
                Response.Bytes = Plyry::MakeJsonBody(Response.Body);
                WeakThis->OnRequestCompleted.Broadcast(Response);
            }
        });
}

void UPlyryApiClientSubsystem::SignIn(const FString& RequestId, const FString& Email, const FString& Password, const TArray<FString>& RequestedScopes)
{
    ++SessionRevision;
    ClearSession(true, true);

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("email"), Email);
    Payload->SetStringField(TEXT("password"), Password);
    Payload->SetStringField(TEXT("app_name"), LinkedAppName);
    TArray<TSharedPtr<FJsonValue>> ScopeValues;
    for (const FString& Scope : RequestedScopes)
    {
        if (!Scope.TrimStartAndEnd().IsEmpty())
        {
            ScopeValues.Add(MakeShared<FJsonValueString>(Scope.TrimStartAndEnd()));
        }
    }
    Payload->SetArrayField(TEXT("scopes"), ScopeValues);
    FString Json;
    SerializeObject(Payload, Json);

    const int32 ThisRevision = SessionRevision;
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    const TArray<uint8> Body = Plyry::MakeJsonBody(Json);
    SendHttp(RequestId, TEXT("/auth/login"), TEXT("POST"), Body, TEXT("application/json; charset=utf-8"), false, TEXT(""), TMap<FString, FString>(), false, 0,
        [WeakThis, ThisRevision](FPlyryApiResponse Response)
        {
            if (!WeakThis.IsValid() || WeakThis->SessionRevision != ThisRevision) { return; }
            if (Response.bSuccess && !WeakThis->ApplyTokenResponse(Response))
            {
                Response.bSuccess = false;
                Response.Error = TEXT("Plyry returned an invalid sign-in token response.");
            }
            else if (!Response.bSuccess)
            {
                Response.Body = UPlyryApiClientSubsystem::SafeAuthBody(Response.Body);
                Response.Bytes = Plyry::MakeJsonBody(Response.Body);
            }
            WeakThis->OnRequestCompleted.Broadcast(Response);
        });
}

void UPlyryApiClientSubsystem::SignUp(const FString& RequestId, const FString& Email, const FString& Password, const FString& Challenge, const FString& Answer, const TArray<FString>& RequestedScopes)
{
    ++SessionRevision;
    ClearSession(true, true);

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("email"), Email);
    Payload->SetStringField(TEXT("password"), Password);
    Payload->SetStringField(TEXT("signup_challenge"), Challenge);
    Payload->SetStringField(TEXT("answer"), Answer);
    Payload->SetStringField(TEXT("app_name"), LinkedAppName);
    TArray<TSharedPtr<FJsonValue>> ScopeValues;
    for (const FString& Scope : RequestedScopes)
    {
        if (!Scope.TrimStartAndEnd().IsEmpty())
        {
            ScopeValues.Add(MakeShared<FJsonValueString>(Scope.TrimStartAndEnd()));
        }
    }
    Payload->SetArrayField(TEXT("scopes"), ScopeValues);
    FString Json;
    SerializeObject(Payload, Json);

    const int32 ThisRevision = SessionRevision;
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    const TArray<uint8> Body = Plyry::MakeJsonBody(Json);
    SendHttp(RequestId, TEXT("/auth/signup"), TEXT("POST"), Body, TEXT("application/json; charset=utf-8"), false, TEXT(""), TMap<FString, FString>(), false, 0,
        [WeakThis, ThisRevision](FPlyryApiResponse Response)
        {
            if (!WeakThis.IsValid() || WeakThis->SessionRevision != ThisRevision) { return; }
            if (Response.bSuccess && !WeakThis->ApplyTokenResponse(Response))
            {
                Response.bSuccess = false;
                Response.Error = TEXT("Plyry returned an invalid sign-up token response.");
            }
            else if (!Response.bSuccess)
            {
                Response.Body = UPlyryApiClientSubsystem::SafeAuthBody(Response.Body);
                Response.Bytes = Plyry::MakeJsonBody(Response.Body);
            }
            WeakThis->OnRequestCompleted.Broadcast(Response);
        });
}

void UPlyryApiClientSubsystem::RefreshSession(const FString& RequestId)
{
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    RefreshAccessToken([WeakThis, RequestId](bool bSuccess, FPlyryApiResponse Response)
    {
        if (!WeakThis.IsValid()) { return; }
        Response.RequestId = RequestId;
        Response.bSuccess = bSuccess;
        WeakThis->OnRequestCompleted.Broadcast(Response);
    });
}

void UPlyryApiClientSubsystem::SignOut(const FString& RequestId)
{
    const FString TokenToRevoke = RefreshToken;
    ++SessionRevision;
    ClearSession(true, true);

    if (TokenToRevoke.IsEmpty())
    {
        FPlyryApiResponse Response;
        Response.RequestId = RequestId;
        Response.bSuccess = true;
        Response.StatusCode = 204;
        OnRequestCompleted.Broadcast(Response);
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("refresh_token"), TokenToRevoke);
    FString Json;
    SerializeObject(Payload, Json);
    const TArray<uint8> Body = Plyry::MakeJsonBody(Json);
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(RequestId, TEXT("/auth/revoke"), TEXT("POST"), Body, TEXT("application/json; charset=utf-8"), false, TEXT(""), TMap<FString, FString>(), false, 0,
        [WeakThis](FPlyryApiResponse Response)
        {
            if (WeakThis.IsValid())
            {
                Response.Body = UPlyryApiClientSubsystem::SafeAuthBody(Response.Body);
                Response.Bytes = Plyry::MakeJsonBody(Response.Body);
                WeakThis->OnRequestCompleted.Broadcast(Response);
            }
        });
}

bool UPlyryApiClientSubsystem::RequestJson(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const FString& JsonBody, bool bAuthenticated, const FString& IdempotencyKey, const TMap<FString, FString>& ExtraHeaders)
{
    if (!ValidateRequestPath(Path) || IsSensitiveAuthPath(Path))
    {
        EmitLocalFailure(RequestId, TEXT("Use the dedicated Plyry authentication methods for auth routes, and pass a relative API path."));
        return false;
    }
    FString HeaderError;
    if (!ValidateExtraHeaders(ExtraHeaders, HeaderError))
    {
        EmitLocalFailure(RequestId, HeaderError);
        return false;
    }

    const TArray<uint8> Body = Plyry::MakeJsonBody(JsonBody);
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(RequestId, Path, VerbToString(Verb), Body, JsonBody.IsEmpty() ? TEXT("") : TEXT("application/json; charset=utf-8"), bAuthenticated, IdempotencyKey, ExtraHeaders, true, 0,
        [WeakThis](FPlyryApiResponse Response)
        {
            if (WeakThis.IsValid()) { WeakThis->OnRequestCompleted.Broadcast(Response); }
        });
    return true;
}

bool UPlyryApiClientSubsystem::RequestBytes(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const TArray<uint8>& Body, const FString& ContentType, bool bAuthenticated, const FString& IdempotencyKey)
{
    if (!ValidateRequestPath(Path) || IsSensitiveAuthPath(Path))
    {
        EmitLocalFailure(RequestId, TEXT("Use the dedicated Plyry authentication methods for auth routes, and pass a relative API path."));
        return false;
    }
    if (!Plyry::IsHeaderSafe(ContentType))
    {
        EmitLocalFailure(RequestId, TEXT("The content type contains invalid header characters."));
        return false;
    }

    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(RequestId, Path, VerbToString(Verb), Body, ContentType, bAuthenticated, IdempotencyKey, TMap<FString, FString>(), true, 0,
        [WeakThis](FPlyryApiResponse Response)
        {
            if (WeakThis.IsValid()) { WeakThis->OnRequestCompleted.Broadcast(Response); }
        });
    return true;
}

bool UPlyryApiClientSubsystem::RequestMultipart(const FString& RequestId, const FString& Path, EPlyryHttpVerb Verb, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, bool bAuthenticated, const FString& IdempotencyKey)
{
    if (!ValidateRequestPath(Path) || IsSensitiveAuthPath(Path))
    {
        EmitLocalFailure(RequestId, TEXT("Use the dedicated Plyry authentication methods for auth routes, and pass a relative API path."));
        return false;
    }

    const FString Boundary = TEXT("PlyryBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<uint8> Body;
    for (const FPlyryFormField& Field : Fields)
    {
        if (!Plyry::IsSafeFormName(Field.Name))
        {
            EmitLocalFailure(RequestId, TEXT("A multipart field has an invalid name."));
            return false;
        }
        Plyry::AppendUtf8(Body, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n"), *Boundary, *Field.Name));
        Plyry::AppendUtf8(Body, Field.Value);
        Plyry::AppendUtf8(Body, TEXT("\r\n"));
    }
    for (const FPlyryFormFile& File : Files)
    {
        if (!Plyry::IsSafeFormName(File.FieldName) || !Plyry::IsHeaderSafe(File.MimeType))
        {
            EmitLocalFailure(RequestId, TEXT("A multipart file has an invalid field name or content type."));
            return false;
        }
        const FString FileName = Plyry::SafeHeaderFilename(File.FileName);
        const FString MimeType = File.MimeType.IsEmpty() ? TEXT("application/octet-stream") : File.MimeType;
        Plyry::AppendUtf8(Body, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"), *Boundary, *File.FieldName, *FileName, *MimeType));
        Body.Append(File.Data);
        Plyry::AppendUtf8(Body, TEXT("\r\n"));
    }
    Plyry::AppendUtf8(Body, FString::Printf(TEXT("--%s--\r\n"), *Boundary));

    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(RequestId, Path, VerbToString(Verb), Body, FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary), bAuthenticated, IdempotencyKey, TMap<FString, FString>(), true, 0,
        [WeakThis](FPlyryApiResponse Response)
        {
            if (WeakThis.IsValid()) { WeakThis->OnRequestCompleted.Broadcast(Response); }
        });
    return true;
}

void UPlyryApiClientSubsystem::GetOpenApiSpecification(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/openapi.json"), EPlyryHttpVerb::Get, TEXT(""), false, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetModels(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/models"), EPlyryHttpVerb::Get, TEXT(""), false, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetMe(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/me"), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetWallet(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/wallet"), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetWalletLedger(const FString& RequestId, int32 Page)
{
    RequestJson(RequestId, FString::Printf(TEXT("/wallet/ledger?page=%d"), FMath::Max(1, Page)), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetPaymentConfig(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/payments/config"), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::QuoteGeneration(const FString& RequestId, const FString& JsonBody)
{
    RequestJson(RequestId, TEXT("/quotes"), EPlyryHttpVerb::Post, JsonBody, true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::CreateGeneration(const FString& RequestId, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, const FString& IdempotencyKey)
{
    RequestMultipart(RequestId, TEXT("/generations"), EPlyryHttpVerb::Post, Fields, Files, true, IdempotencyKey);
}

void UPlyryApiClientSubsystem::ListGenerations(const FString& RequestId, const FString& QueryString)
{
    RequestJson(RequestId, QueryPath(TEXT("/generations"), QueryString), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetGeneration(const FString& RequestId, const FString& Hash)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestJson(RequestId, TEXT("/generations/") + Hash, EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::DownloadGenerationMedia(const FString& RequestId, const FString& Hash, bool bScene)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    const FString Path = bScene ? TEXT("/media/") + Hash + TEXT("/scene") : TEXT("/media/") + Hash;
    RequestBytes(RequestId, Path, EPlyryHttpVerb::Get, TArray<uint8>(), TEXT(""), true, TEXT(""));
}

void UPlyryApiClientSubsystem::AcknowledgeGenerationCache(const FString& RequestId, const FString& Hash, const FString& CacheToken, const FString& Sha256)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("cache_token"), CacheToken);
    Payload->SetStringField(TEXT("sha256"), Sha256);
    FString Json;
    SerializeObject(Payload, Json);
    RequestJson(RequestId, TEXT("/generations/") + Hash + TEXT("/cache"), EPlyryHttpVerb::Post, Json, true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::CreateWalletTopup(const FString& RequestId, const FString& JsonBody, const FString& IdempotencyKey)
{
    RequestJson(RequestId, TEXT("/wallet/topups"), EPlyryHttpVerb::Post, JsonBody, true, IdempotencyKey, TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetWalletTopupStatus(const FString& RequestId, const FString& RequestKey)
{
    if (!Plyry::IsSafeRouteSegment(RequestKey)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestJson(RequestId, TEXT("/wallet/topups/requests/") + RequestKey, EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::ReconcileWalletTopup(const FString& RequestId, const FString& JsonBody, const FString& IdempotencyKey)
{
    RequestJson(RequestId, TEXT("/wallet/topups/reconcile"), EPlyryHttpVerb::Post, JsonBody, true, IdempotencyKey, TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::GetPlant3DToRevitPrice(const FString& RequestId)
{
    RequestJson(RequestId, TEXT("/conversions/plant3d-to-revit/price"), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::CreatePlant3DToRevitJob(const FString& RequestId, const TArray<FPlyryFormField>& Fields, const TArray<FPlyryFormFile>& Files, const FString& IdempotencyKey)
{
    RequestMultipart(RequestId, TEXT("/conversions/plant3d-to-revit/jobs"), EPlyryHttpVerb::Post, Fields, Files, true, IdempotencyKey);
}

void UPlyryApiClientSubsystem::GetPlant3DToRevitJob(const FString& RequestId, const FString& Hash)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestJson(RequestId, TEXT("/conversions/plant3d-to-revit/jobs/") + Hash, EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::SearchPlant3DToRevitButtons(const FString& RequestId, const FString& Hash, const FString& QueryString)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestJson(RequestId, QueryPath(TEXT("/conversions/plant3d-to-revit/jobs/") + Hash + TEXT("/buttons"), QueryString), EPlyryHttpVerb::Get, TEXT(""), true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::RevisePlant3DToRevitMapping(const FString& RequestId, const FString& Hash, const FString& JsonBody)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestJson(RequestId, TEXT("/conversions/plant3d-to-revit/jobs/") + Hash + TEXT("/mapping"), EPlyryHttpVerb::Post, JsonBody, true, TEXT(""), TMap<FString, FString>());
}

void UPlyryApiClientSubsystem::DownloadPlant3DToRevitMapping(const FString& RequestId, const FString& Hash)
{
    if (!Plyry::IsSafeRouteSegment(Hash)) { EmitLocalFailure(RequestId, TEXT("The route identifier contains invalid characters.")); return; }
    RequestBytes(RequestId, TEXT("/conversions/plant3d-to-revit/jobs/") + Hash + TEXT("/download"), EPlyryHttpVerb::Get, TArray<uint8>(), TEXT(""), true, TEXT(""));
}

namespace Plyry
{
    static bool IsHeaderNameSafe(const FString& Name)
    {
        if (Name.IsEmpty()) { return false; }
        for (const TCHAR Character : Name)
        {
            const bool bAllowed =
                (Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('-') || Character == TEXT('_');
            if (!bAllowed) { return false; }
        }
        return true;
    }
}

void UPlyryApiClientSubsystem::SendHttp(const FString& RequestId, const FString& Path, const FString& Verb, const TArray<uint8>& Body, const FString& ContentType, bool bAuthenticated, const FString& IdempotencyKey, const TMap<FString, FString>& ExtraHeaders, bool bAllowRefreshRetry, int32 RetryCount, FRequestCallback Completion)
{
    if (!Plyry::IsHeaderSafe(IdempotencyKey))
    {
        FPlyryApiResponse Failure;
        Failure.RequestId = RequestId;
        Failure.Error = TEXT("The idempotency key contains invalid header characters.");
        if (Completion) { Completion(Failure); }
        return;
    }

    FString Url;
    if (!BuildUrl(Path, Url))
    {
        FPlyryApiResponse Failure;
        Failure.RequestId = RequestId;
        Failure.Error = TEXT("The Plyry API URL or relative path is invalid.");
        if (Completion) { Completion(Failure); }
        return;
    }

    if (bAuthenticated && AccessToken.IsEmpty() && ApiKey.IsEmpty() && !RefreshToken.IsEmpty() && bAllowRefreshRetry && RetryCount == 0)
    {
        TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
        RefreshAccessToken([WeakThis, RequestId, Path, Verb, Body, ContentType, IdempotencyKey, ExtraHeaders, bAllowRefreshRetry, RetryCount, Completion](bool bRefreshed, FPlyryApiResponse)
        {
            if (!WeakThis.IsValid()) { return; }
            if (bRefreshed)
            {
                WeakThis->SendHttp(RequestId, Path, Verb, Body, ContentType, true, IdempotencyKey, ExtraHeaders, bAllowRefreshRetry, RetryCount + 1, Completion);
            }
            else if (Completion)
            {
                FPlyryApiResponse Failure;
                Failure.RequestId = RequestId;
                Failure.StatusCode = 401;
                Failure.Error = TEXT("Your Plyry session has expired. Sign in again.");
                Completion(Failure);
            }
        });
        return;
    }

    if (bAuthenticated && AccessToken.IsEmpty() && ApiKey.IsEmpty())
    {
        FPlyryApiResponse Failure;
        Failure.RequestId = RequestId;
        Failure.Error = TEXT("Sign in to Plyry or set a trusted-server API key before making this request.");
        if (Completion) { Completion(Failure); }
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(Verb);
    Request->SetTimeout(90.0f);
    Request->SetHeader(TEXT("Accept"), TEXT("application/json, application/octet-stream"));
    if (!ContentType.IsEmpty()) { Request->SetHeader(TEXT("Content-Type"), ContentType); }
    if (!IdempotencyKey.IsEmpty()) { Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey); }
    for (const TPair<FString, FString>& Header : ExtraHeaders) { Request->SetHeader(Header.Key, Header.Value); }
    if (bAuthenticated)
    {
        const FString& Token = AccessToken.IsEmpty() ? ApiKey : AccessToken;
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    }
    if (Body.Num() > 0)
    {
        Request->SetContent(TArray<uint8>(Body));
    }

    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, RequestId, Path, Verb, Body, ContentType, IdempotencyKey, ExtraHeaders, bAuthenticated, bAllowRefreshRetry, RetryCount, Completion]
        (FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully) mutable
        {
            if (!WeakThis.IsValid()) { return; }
            FPlyryApiResponse Response;
            Response.RequestId = RequestId;
            if (HttpResponse.IsValid())
            {
                Response.StatusCode = HttpResponse->GetResponseCode();
                Response.ContentType = HttpResponse->GetHeader(TEXT("Content-Type"));
                for (const FString& HeaderLine : HttpResponse->GetAllHeaders())
                {
                    FString HeaderName;
                    FString HeaderValue;
                    if (HeaderLine.Split(TEXT(":"), &HeaderName, &HeaderValue) &&
                        !HeaderName.Equals(TEXT("Set-Cookie"), ESearchCase::IgnoreCase) &&
                        !HeaderName.Equals(TEXT("Authorization"), ESearchCase::IgnoreCase))
                    {
                        Response.Headers.Add(HeaderName.TrimStartAndEnd(), HeaderValue.TrimStartAndEnd());
                    }
                }
                Response.Bytes = HttpResponse->GetContent();
                if (Plyry::IsJsonContentType(Response.ContentType))
                {
                    Response.Body = HttpResponse->GetContentAsString();
                }
                Response.bSuccess = bConnectedSuccessfully && Response.StatusCode >= 200 && Response.StatusCode < 300;
                if (!Response.bSuccess)
                {
                    Response.Error = Plyry::ExtractErrorMessage(Response.Body);
                    if (Response.Error.IsEmpty())
                    {
                        Response.Error = bConnectedSuccessfully
                            ? FString::Printf(TEXT("Plyry returned HTTP %d."), Response.StatusCode)
                            : TEXT("The connection to Plyry failed.");
                    }
                }
            }
            else
            {
                Response.Error = TEXT("The connection to Plyry failed before a response was received.");
            }

            if (bAuthenticated && bAllowRefreshRetry && RetryCount == 0 && Response.StatusCode == 401 && !WeakThis->RefreshToken.IsEmpty())
            {
                WeakThis->RefreshAccessToken([WeakThis, RequestId, Path, Verb, Body, ContentType, IdempotencyKey, ExtraHeaders, bAllowRefreshRetry, RetryCount, Completion, Response](bool bRefreshed, FPlyryApiResponse)
                {
                    if (!WeakThis.IsValid()) { return; }
                    if (bRefreshed)
                    {
                        WeakThis->SendHttp(RequestId, Path, Verb, Body, ContentType, true, IdempotencyKey, ExtraHeaders, bAllowRefreshRetry, RetryCount + 1, Completion);
                    }
                    else if (Completion)
                    {
                        Completion(Response);
                    }
                });
                return;
            }
            if (Completion) { Completion(Response); }
        });

    if (!Request->ProcessRequest())
    {
        FPlyryApiResponse Failure;
        Failure.RequestId = RequestId;
        Failure.Error = TEXT("Unreal could not start the HTTP request.");
        if (Completion) { Completion(Failure); }
    }
}

void UPlyryApiClientSubsystem::RefreshAccessToken(FRefreshCallback Completion)
{
    if (bRefreshInProgress)
    {
        if (Completion) { RefreshWaiters.Add(MoveTemp(Completion)); }
        return;
    }
    if (RefreshToken.IsEmpty())
    {
        FPlyryApiResponse Failure;
        Failure.Error = TEXT("No Plyry refresh token is available.");
        if (Completion) { Completion(false, Failure); }
        return;
    }

    bRefreshInProgress = true;
    if (Completion) { RefreshWaiters.Add(MoveTemp(Completion)); }
    const int32 ThisRevision = SessionRevision;
    const FString RefreshTokenToUse = RefreshToken;
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("refresh_token"), RefreshTokenToUse);
    FString Json;
    SerializeObject(Payload, Json);
    const TArray<uint8> Body = Plyry::MakeJsonBody(Json);
    TWeakObjectPtr<UPlyryApiClientSubsystem> WeakThis(this);
    SendHttp(TEXT("internal.refresh"), TEXT("/auth/refresh"), TEXT("POST"), Body, TEXT("application/json; charset=utf-8"), false, TEXT(""), TMap<FString, FString>(), false, 0,
        [WeakThis, ThisRevision](FPlyryApiResponse Response)
        {
            if (!WeakThis.IsValid()) { return; }
            const bool bSuccess = Response.bSuccess && WeakThis->SessionRevision == ThisRevision && WeakThis->ApplyTokenResponse(Response);
            if (WeakThis->SessionRevision != ThisRevision)
            {
                Response.bSuccess = false;
                Response.Error = TEXT("The refresh response was discarded because the local session changed.");
            }
            else if (!bSuccess && Response.StatusCode == 401)
            {
                WeakThis->ClearSession(true, true);
            }
            Response.Body = UPlyryApiClientSubsystem::SafeAuthBody(Response.Body);
            Response.Bytes = Plyry::MakeJsonBody(Response.Body);
            WeakThis->CompleteRefresh(bSuccess, Response);
        });
}

void UPlyryApiClientSubsystem::CompleteRefresh(bool bSuccess, const FPlyryApiResponse& Response)
{
    bRefreshInProgress = false;
    TArray<FRefreshCallback> Waiters = MoveTemp(RefreshWaiters);
    RefreshWaiters.Reset();
    for (FRefreshCallback& Waiter : Waiters)
    {
        if (Waiter) { Waiter(bSuccess, Response); }
    }
}

bool UPlyryApiClientSubsystem::ApplyTokenResponse(FPlyryApiResponse& Response)
{
    TSharedPtr<FJsonObject> Object;
    if (!ParseJsonObject(Response.Body, Object) || !Object.IsValid())
    {
        Response.Body.Reset();
        Response.Bytes.Reset();
        return false;
    }
    FString NewAccessToken;
    FString NewRefreshToken;
    if (!Object->TryGetStringField(TEXT("access_token"), NewAccessToken) ||
        !Object->TryGetStringField(TEXT("refresh_token"), NewRefreshToken) ||
        NewAccessToken.IsEmpty() || NewRefreshToken.IsEmpty() ||
        !Plyry::IsHeaderSafe(NewAccessToken) || !Plyry::IsHeaderSafe(NewRefreshToken))
    {
        Response.Body = SafeAuthBody(Response.Body);
        Response.Bytes = Plyry::MakeJsonBody(Response.Body);
        return false;
    }

    AccessToken = NewAccessToken;
    RefreshToken = NewRefreshToken;
    double ExpiresIn = 3600.0;
    Object->TryGetNumberField(TEXT("expires_in"), ExpiresIn);
    AccessTokenExpiresAt = FDateTime::UtcNow() + FTimespan::FromSeconds(FMath::Max(1.0, ExpiresIn));

    GrantedScopes.Reset();
    const TArray<TSharedPtr<FJsonValue>>* ScopeArray = nullptr;
    if (Object->TryGetArrayField(TEXT("scopes"), ScopeArray) && ScopeArray != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Scope : *ScopeArray)
        {
            if (Scope.IsValid()) { GrantedScopes.Add(Scope->AsString()); }
        }
    }

    UserJson.Reset();
    const TSharedPtr<FJsonObject>* UserObject = nullptr;
    if (Object->TryGetObjectField(TEXT("user"), UserObject) && UserObject != nullptr && UserObject->IsValid())
    {
        Plyry::RemoveSensitiveTokenFields(*UserObject);
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&UserJson);
        FJsonSerializer::Serialize(UserObject->ToSharedRef(), Writer);
    }

    if (CredentialStore != nullptr)
    {
        UPlyryCredentialStore::Execute_SaveRefreshToken(CredentialStore, RefreshToken);
    }
    else
    {
        OnCredentialWarning.Broadcast(TEXT("The refresh token is stored in memory only. Assign a platform secure credential store to restore sessions after restart."));
    }

    Response.Body = SafeAuthBody(Response.Body);
    Response.Bytes = Plyry::MakeJsonBody(Response.Body);
    Response.bSuccess = true;
    BroadcastSessionChanged();
    return true;
}

void UPlyryApiClientSubsystem::ClearSession(bool bClearPersistentToken, bool bBroadcast)
{
    AccessToken.Reset();
    RefreshToken.Reset();
    UserJson.Reset();
    GrantedScopes.Reset();
    AccessTokenExpiresAt = FDateTime();
    if (bClearPersistentToken && CredentialStore != nullptr)
    {
        UPlyryCredentialStore::Execute_ClearRefreshToken(CredentialStore);
    }
    if (bBroadcast) { BroadcastSessionChanged(); }
}

void UPlyryApiClientSubsystem::EmitLocalFailure(const FString& RequestId, const FString& Message)
{
    FPlyryApiResponse Response;
    Response.RequestId = RequestId;
    Response.Error = Message;
    OnRequestCompleted.Broadcast(Response);
}

void UPlyryApiClientSubsystem::BroadcastSessionChanged() const
{
    OnSessionChanged.Broadcast(GetSessionSnapshot());
}

bool UPlyryApiClientSubsystem::BuildUrl(const FString& Path, FString& OutUrl) const
{
    if (!ValidateBaseUrl(ApiBaseUrl) || !ValidateRequestPath(Path)) { return false; }
    FString Base = ApiBaseUrl;
    while (Base.EndsWith(TEXT("/"))) { Base.LeftChopInline(1); }
    OutUrl = Base + Path;
    return true;
}

bool UPlyryApiClientSubsystem::ValidateBaseUrl(const FString& Url) const
{
    const FString Value = Url.TrimStartAndEnd();
    if (Value.IsEmpty() || !Plyry::IsHeaderSafe(Value) || Value.Contains(TEXT(" ")) || Value.Contains(TEXT("\t")) ||
        Value.Contains(TEXT("\r")) || Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\\")) ||
        Value.Contains(TEXT("@")) || Value.Contains(TEXT("?")) || Value.Contains(TEXT("#")))
    {
        return false;
    }

    const int32 SchemeEnd = Value.Find(TEXT("://"));
    if (SchemeEnd == INDEX_NONE)
    {
        return false;
    }

    const FString Scheme = Value.Left(SchemeEnd);
    const bool bHttps = Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase);
    const bool bHttp = Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase);
    if (!bHttps && !bHttp)
    {
        return false;
    }

    const FString AuthorityAndPath = Value.Mid(SchemeEnd + 3);
    const int32 PathStart = AuthorityAndPath.Find(TEXT("/"));
    const FString Authority = PathStart == INDEX_NONE ? AuthorityAndPath : AuthorityAndPath.Left(PathStart);
    const FString BasePath = PathStart == INDEX_NONE ? FString() : AuthorityAndPath.Mid(PathStart);
    if (Authority.IsEmpty())
    {
        return false;
    }

    if (bHttp)
    {
        const bool bLocalHost = Authority.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) ||
            Authority.Equals(TEXT("127.0.0.1"), ESearchCase::IgnoreCase) ||
            Authority.StartsWith(TEXT("localhost:"), ESearchCase::IgnoreCase) ||
            Authority.StartsWith(TEXT("127.0.0.1:"), ESearchCase::IgnoreCase);
        if (!bLocalHost)
        {
            return false;
        }
        const int32 PortSeparator = Authority.Find(TEXT(":"));
        if (PortSeparator != INDEX_NONE)
        {
            const FString Port = Authority.Mid(PortSeparator + 1);
            if (!Port.IsNumeric() || Port.IsEmpty() || FCString::Atoi(*Port) < 1 || FCString::Atoi(*Port) > 65535)
            {
                return false;
            }
        }
    }

    return !BasePath.Contains(TEXT("..")) && !BasePath.Contains(TEXT("%")) &&
        !BasePath.Contains(TEXT("\\")) && !BasePath.Contains(TEXT("//"));
}
bool UPlyryApiClientSubsystem::ValidateRequestPath(const FString& Path) const
{
    for (const TCHAR Character : Path)
    {
        if (Character <= 0x20 || Character == 0x7f) { return false; }
    }
    if (!Path.StartsWith(TEXT("/")) || Path.StartsWith(TEXT("//")) || Path.Contains(TEXT("://")) ||
        Path.Contains(TEXT("\\")) || Path.Contains(TEXT("#")) || Path.Contains(TEXT("\r")) || Path.Contains(TEXT("\n")))
    {
        return false;
    }
    const int32 QueryIndex = Path.Find(TEXT("?"));
    const FString Route = QueryIndex == INDEX_NONE ? Path : Path.Left(QueryIndex);
    return !Route.Contains(TEXT("..")) && !Route.Contains(TEXT("%")) && !Route.Contains(TEXT("//"));
}

bool UPlyryApiClientSubsystem::ValidateExtraHeaders(const TMap<FString, FString>& Headers, FString& OutError) const
{
    static const TSet<FString> RestrictedHeaders = {
        TEXT("authorization"), TEXT("host"), TEXT("content-type"), TEXT("content-length"),
        TEXT("cookie"), TEXT("proxy-authorization"), TEXT("idempotency-key")
    };
    for (const TPair<FString, FString>& Header : Headers)
    {
        if (!Plyry::IsHeaderNameSafe(Header.Key) || !Plyry::IsHeaderSafe(Header.Value) ||
            RestrictedHeaders.Contains(Header.Key.ToLower()))
        {
            OutError = TEXT("An extra header is invalid or attempts to override a protected header.");
            return false;
        }
    }
    return true;
}

bool UPlyryApiClientSubsystem::IsSensitiveAuthPath(const FString& Path) const
{
    FString Route = Path;
    const int32 QueryIndex = Route.Find(TEXT("?"));
    if (QueryIndex != INDEX_NONE) { Route = Route.Left(QueryIndex); }
    Route = Route.ToLower();
    while (Route.Len() > 1 && Route.EndsWith(TEXT("/"))) { Route.LeftChopInline(1); }
    return Route == TEXT("/auth/login") || Route == TEXT("/auth/signup") ||
        Route == TEXT("/auth/signup/challenges") || Route == TEXT("/auth/refresh") ||
        Route == TEXT("/auth/revoke");
}

bool UPlyryApiClientSubsystem::SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
{
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    return FJsonSerializer::Serialize(Object, Writer);
}

bool UPlyryApiClientSubsystem::ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
{
    if (Json.IsEmpty()) { return false; }
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

FString UPlyryApiClientSubsystem::SafeAuthBody(const FString& Json)
{
    TSharedPtr<FJsonObject> Object;
    if (!ParseJsonObject(Json, Object) || !Object.IsValid()) { return FString(); }
    Plyry::RemoveSensitiveTokenFields(Object);
    FString SafeJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SafeJson);
    FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    return SafeJson;
}

FString UPlyryApiClientSubsystem::VerbToString(EPlyryHttpVerb Verb)
{
    switch (Verb)
    {
    case EPlyryHttpVerb::Post: return TEXT("POST");
    case EPlyryHttpVerb::Put: return TEXT("PUT");
    case EPlyryHttpVerb::Patch: return TEXT("PATCH");
    case EPlyryHttpVerb::Delete: return TEXT("DELETE");
    case EPlyryHttpVerb::Get:
    default: return TEXT("GET");
    }
}

FString UPlyryApiClientSubsystem::QueryPath(const FString& Path, const FString& QueryString)
{
    if (QueryString.IsEmpty()) { return Path; }
    return Path + (QueryString.StartsWith(TEXT("?")) ? QueryString : TEXT("?") + QueryString);
}
