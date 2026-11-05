#include "PlyryAccountWidget.h"

#include "PlyryAccountTheme.h"
#include "PlyryApiClientSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Json.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UPlyryAccountWidget::UPlyryAccountWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    HeaderText = FText::FromString(TEXT("Plyry account"));
    EmailHintText = FText::FromString(TEXT("Email"));
    PasswordHintText = FText::FromString(TEXT("Password"));
    ChallengeAnswerHintText = FText::FromString(TEXT("Signup answer"));
    SignInButtonText = FText::FromString(TEXT("Sign in"));
    RequestChallengeButtonText = FText::FromString(TEXT("Get signup question"));
    SignUpButtonText = FText::FromString(TEXT("Create account"));
    RefreshButtonText = FText::FromString(TEXT("Refresh account"));
    SignOutButtonText = FText::FromString(TEXT("Sign out"));
}

void UPlyryAccountWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (bBuildDefaultLayout && WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
    {
        BuildLocalLayout();
    }
}

void UPlyryAccountWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RequestIdPrefix = FString::Printf(TEXT("plyry.ui.%d.%s."), GetUniqueID(), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    BindClient();
    if (Client != nullptr) { Client->RestoreSession(MakeRequestId(TEXT("restore"))); }
}

void UPlyryAccountWidget::NativeDestruct()
{
    if (Client != nullptr)
    {
        Client->OnRequestCompleted.RemoveDynamic(this, &UPlyryAccountWidget::HandleRequestCompleted);
    }
    Super::NativeDestruct();
}

void UPlyryAccountWidget::BindClient()
{
    if (GetGameInstance() == nullptr)
    {
        return;
    }
    Client = GetGameInstance()->GetSubsystem<UPlyryApiClientSubsystem>();
    if (Client != nullptr)
    {
        Client->OnRequestCompleted.AddUniqueDynamic(this, &UPlyryAccountWidget::HandleRequestCompleted);
    }
}

void UPlyryAccountWidget::BuildLocalLayout()
{
    if (WidgetTree == nullptr)
    {
        return;
    }

    USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlyryPanelSize"));
    PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlyryPanel"));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlyryAccountLayout"));
    WidgetTree->RootWidget = SizeBox;
    SizeBox->SetWidthOverride(Theme != nullptr ? Theme->PanelWidth : 420.0f);
    SizeBox->SetContent(PanelBorder);
    PanelBorder->SetPadding(Theme != nullptr ? Theme->PanelPadding : FMargin(24.0f));
    PanelBorder->SetBrushColor(Theme != nullptr ? Theme->PanelColor : FLinearColor(0.035f, 0.045f, 0.065f, 0.98f));
    PanelBorder->SetContent(Layout);

    UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlyryHeader"));
    Header->SetText(HeaderText);
    ApplyTextStyle(Header, true);
    Layout->AddChildToVerticalBox(Header);

    EmailInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PlyryEmail"));
    EmailInput->SetHintText(EmailHintText);
    ApplyInputStyle(EmailInput);
    Layout->AddChildToVerticalBox(EmailInput)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

    PasswordInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PlyryPassword"));
    PasswordInput->SetHintText(PasswordHintText);
    PasswordInput->SetIsPassword(true);
    ApplyInputStyle(PasswordInput);
    Layout->AddChildToVerticalBox(PasswordInput)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

    UButton* SignInButton = AddButton(Layout, SignInButtonText, TEXT("PlyrySignIn"));
    if (SignInButton != nullptr) { SignInButton->OnClicked.AddDynamic(this, &UPlyryAccountWidget::HandleSignInClicked); }
    UButton* ChallengeButton = AddButton(Layout, RequestChallengeButtonText, TEXT("PlyrySignupChallenge"));
    if (ChallengeButton != nullptr) { ChallengeButton->OnClicked.AddDynamic(this, &UPlyryAccountWidget::HandleChallengeClicked); }

    QuestionLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlyrySignupQuestion"));
    QuestionLabel->SetAutoWrapText(true);
    ApplyTextStyle(QuestionLabel, false);
    Layout->AddChildToVerticalBox(QuestionLabel)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

    AnswerInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PlyrySignupAnswer"));
    AnswerInput->SetHintText(ChallengeAnswerHintText);
    ApplyInputStyle(AnswerInput);
    Layout->AddChildToVerticalBox(AnswerInput)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

    UButton* SignUpButton = AddButton(Layout, SignUpButtonText, TEXT("PlyrySignUp"));
    if (SignUpButton != nullptr) { SignUpButton->OnClicked.AddDynamic(this, &UPlyryAccountWidget::HandleSignUpClicked); }

    StatusLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlyryStatus"));
    StatusLabel->SetAutoWrapText(true);
    ApplyTextStyle(StatusLabel, false);
    Layout->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

    AccountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlyryAccount"));
    AccountLabel->SetAutoWrapText(true);
    ApplyTextStyle(AccountLabel, false);
    if (Theme != nullptr) { AccountLabel->SetColorAndOpacity(FSlateColor(Theme->MutedColor)); }
    Layout->AddChildToVerticalBox(AccountLabel)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

    UButton* RefreshButton = AddButton(Layout, RefreshButtonText, TEXT("PlyryRefresh"));
    if (RefreshButton != nullptr) { RefreshButton->OnClicked.AddDynamic(this, &UPlyryAccountWidget::HandleRefreshClicked); }
    UButton* SignOutButton = AddButton(Layout, SignOutButtonText, TEXT("PlyrySignOut"));
    if (SignOutButton != nullptr) { SignOutButton->OnClicked.AddDynamic(this, &UPlyryAccountWidget::HandleSignOutClicked); }

    USpacer* BottomSpace = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("PlyryBottomSpace"));
    BottomSpace->SetSize(FVector2D(1.0f, 4.0f));
    Layout->AddChildToVerticalBox(BottomSpace);
}

UButton* UPlyryAccountWidget::AddButton(UVerticalBox* Parent, const FText& Text, const FName& Name)
{
    if (WidgetTree == nullptr || Parent == nullptr)
    {
        return nullptr;
    }
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    if (Theme != nullptr)
    {
        FButtonStyle Style = Button->GetStyle();
        if (Theme->bOverrideButtonStyle) { Style = Theme->ButtonStyle; }
        Style.Normal.TintColor = FSlateColor(Theme->SurfaceColor);
        Style.Hovered.TintColor = FSlateColor(Theme->AccentColor);
        Style.Pressed.TintColor = FSlateColor(Theme->AccentColor);
        Button->SetStyle(Style);
    }
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Label"))));
    Label->SetText(Text);
    ApplyTextStyle(Label, false);
    if (Theme != nullptr) { Label->SetColorAndOpacity(FSlateColor(Theme->TextColor)); }
    Button->AddChild(Label);
    Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    return Button;
}

void UPlyryAccountWidget::SignIn()
{
    const TArray<FString> Scopes = { TEXT("account:read"), TEXT("wallet:read") };
    SignInWithCredentials(ReadInput(EmailInput).ToString(), ReadInput(PasswordInput).ToString(), Scopes);
}

void UPlyryAccountWidget::SignInWithCredentials(const FString& Email, const FString& Password, const TArray<FString>& RequestedScopes)
{
    if (Client == nullptr) { BindClient(); }
    if (Client == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Plyry is unavailable in this game instance.")));
        return;
    }
    SetStatus(FText::FromString(TEXT("Signing in...")));
    Client->SignIn(MakeRequestId(TEXT("signin")), Email, Password, RequestedScopes);
}
void UPlyryAccountWidget::RequestSignupChallenge()
{
    RequestSignupChallengeForEmail(ReadInput(EmailInput).ToString());
}

void UPlyryAccountWidget::RequestSignupChallengeForEmail(const FString& Email)
{
    if (Client == nullptr) { BindClient(); }
    if (Client == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Plyry is unavailable in this game instance.")));
        return;
    }
    SetStatus(FText::FromString(TEXT("Getting signup question...")));
    Client->RequestSignupChallenge(MakeRequestId(TEXT("challenge")), Email);
}
void UPlyryAccountWidget::SignUp()
{
    const TArray<FString> Scopes = { TEXT("account:read"), TEXT("wallet:read") };
    SignUpWithChallenge(ReadInput(EmailInput).ToString(), ReadInput(PasswordInput).ToString(), ActiveSignupChallenge, ReadInput(AnswerInput).ToString(), Scopes);
}

void UPlyryAccountWidget::SignUpWithChallenge(const FString& Email, const FString& Password, const FString& Challenge, const FString& Answer, const TArray<FString>& RequestedScopes)
{
    if (Client == nullptr) { BindClient(); }
    if (Client == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Plyry is unavailable in this game instance.")));
        return;
    }
    if (Challenge.IsEmpty())
    {
        SetStatus(FText::FromString(TEXT("Request the signup question before creating an account.")));
        return;
    }
    SetStatus(FText::FromString(TEXT("Creating account...")));
    Client->SignUp(MakeRequestId(TEXT("signup")), Email, Password, Challenge, Answer, RequestedScopes);
}
void UPlyryAccountWidget::RefreshAccount()
{
    if (Client == nullptr) { BindClient(); }
    if (Client == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Plyry is unavailable in this game instance.")));
        return;
    }
    SetStatus(FText::FromString(TEXT("Refreshing account...")));
    Client->GetMe(MakeRequestId(TEXT("me")));
}

void UPlyryAccountWidget::SignOut()
{
    if (Client == nullptr) { BindClient(); }
    if (Client == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Plyry is unavailable in this game instance.")));
        return;
    }
    SetStatus(FText::FromString(TEXT("Signing out...")));
    Client->SignOut(MakeRequestId(TEXT("signout")));
}

void UPlyryAccountWidget::HandleSignInClicked() { SignIn(); }
void UPlyryAccountWidget::HandleChallengeClicked() { RequestSignupChallenge(); }
void UPlyryAccountWidget::HandleSignUpClicked() { SignUp(); }
void UPlyryAccountWidget::HandleRefreshClicked() { RefreshAccount(); }
void UPlyryAccountWidget::HandleSignOutClicked() { SignOut(); }

void UPlyryAccountWidget::HandleRequestCompleted(const FPlyryApiResponse& Response)
{
    if (Response.RequestId == MakeRequestId(TEXT("restore")))
    {
        if (Response.bSuccess && Client != nullptr && Client->GetSessionSnapshot().bAuthenticated) { RefreshAccount(); }
        else if (!Response.bSuccess && !Response.Error.IsEmpty()) { SetStatus(FText::FromString(Response.Error)); }
        else { SetStatus(FText::FromString(TEXT("Sign in to your Plyry account."))); }
        return;
    }

    if (Response.RequestId == MakeRequestId(TEXT("challenge")))
    {
        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.Body);
        FString Challenge;
        FString Question;
        if (Response.bSuccess && FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid() &&
            Object->TryGetStringField(TEXT("signup_challenge"), Challenge) &&
            Object->TryGetStringField(TEXT("question"), Question))
        {
            ActiveSignupChallenge = Challenge;
            if (QuestionLabel != nullptr) { QuestionLabel->SetText(FText::FromString(Question)); }
            OnSignupQuestionChanged.Broadcast(FText::FromString(Question));
            OnSignupChallengeReceived.Broadcast(Response.Body);
            SetStatus(FText::FromString(TEXT("Answer the signup question, then create your account.")));
        }
        else
        {
            SetStatus(FText::FromString(Response.Error.IsEmpty() ? TEXT("Could not get the signup question.") : Response.Error));
        }
        return;
    }

    if (Response.RequestId == MakeRequestId(TEXT("signin")) || Response.RequestId == MakeRequestId(TEXT("signup")))
    {
        SetStatus(FText::FromString(Response.bSuccess ? TEXT("Signed in.") :
            (Response.Error.IsEmpty() ? TEXT("Authentication failed.") : Response.Error)));
        if (Response.bSuccess)
        {
            if (PasswordInput != nullptr) { PasswordInput->SetText(FText::GetEmpty()); }
            ActiveSignupChallenge.Reset();
            RefreshAccount();
        }
        return;
    }

    if (Response.RequestId == MakeRequestId(TEXT("me")))
    {
        if (Response.bSuccess)
        {
            if (AccountLabel != nullptr) { AccountLabel->SetText(FText::FromString(Response.Body)); }
            OnAccountDataChanged.Broadcast(Response.Body);
            SetStatus(FText::FromString(TEXT("Plyry account is ready.")));
        }
        else
        {
            SetStatus(FText::FromString(Response.Error.IsEmpty() ? TEXT("Could not refresh the account.") : Response.Error));
        }
        return;
    }

    if (Response.RequestId == MakeRequestId(TEXT("signout")))
    {
        if (AccountLabel != nullptr) { AccountLabel->SetText(FText::GetEmpty()); }
        if (PasswordInput != nullptr) { PasswordInput->SetText(FText::GetEmpty()); }
        SetStatus(FText::FromString(Response.bSuccess ? TEXT("Signed out.") : TEXT("Signed out locally. Plyry could not confirm token revocation.")));
    }
}

void UPlyryAccountWidget::ApplyTextStyle(UTextBlock* Text, bool bHeader) const
{
    if (Text != nullptr && Theme != nullptr)
    {
        Text->SetColorAndOpacity(FSlateColor(Theme->TextColor));
        Text->SetFont(bHeader ? Theme->HeaderFont : Theme->BodyFont);
    }
}

void UPlyryAccountWidget::ApplyInputStyle(UEditableTextBox* Input) const
{
    if (Input != nullptr && Theme != nullptr)
    {
        FEditableTextBoxStyle Style = Input->GetWidgetStyle();
        if (Theme->bOverrideInputStyle) { Style = Theme->InputStyle; }
        Style.BackgroundColor = FSlateColor(Theme->SurfaceColor);
        Style.ForegroundColor = FSlateColor(Theme->TextColor);
        Style.FocusedForegroundColor = FSlateColor(Theme->TextColor);
        Style.BackgroundImageNormal.TintColor = FSlateColor(Theme->SurfaceColor);
        Style.BackgroundImageHovered.TintColor = FSlateColor(Theme->AccentColor);
        Style.BackgroundImageFocused.TintColor = FSlateColor(Theme->SurfaceColor);
        Style.BackgroundImageReadOnly.TintColor = FSlateColor(Theme->SurfaceColor);
        Input->SetWidgetStyle(Style);
    }
}

FString UPlyryAccountWidget::MakeRequestId(const FString& Suffix) const
{
    return RequestIdPrefix + Suffix;
}

void UPlyryAccountWidget::SetStatus(const FText& Text)
{
    if (StatusLabel != nullptr) { StatusLabel->SetText(Text); }
    OnStatusChanged.Broadcast(Text);
}

FText UPlyryAccountWidget::ReadInput(const UEditableTextBox* Input) const
{
    return Input != nullptr ? Input->GetText() : FText::GetEmpty();
}
