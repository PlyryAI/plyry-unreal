# Integration notes

## Blueprint request flow

1. From a Game Instance or UMG widget, get the Game Instance subsystem of class `PlyryApiClientSubsystem`.
2. Bind `OnRequestCompleted` once.
3. Call `GetModels` or another route method with a unique ID such as `models.refresh`.
4. In the completion event, compare `RequestId`; inspect `bSuccess`, `StatusCode`, `Body`, `Bytes`, and `Error`.

Keep the subsystem reference for the lifetime of the screen and remove event bindings when the screen is destroyed.

## Native C++

```cpp
UPlyryApiClientSubsystem* Client =
    GetGameInstance()->GetSubsystem<UPlyryApiClientSubsystem>();

Client->OnRequestCompleted.AddUniqueDynamic(
    this, &UMyWidget::HandlePlyryResponse);

Client->GetWallet(TEXT("wallet.refresh"));
```

A custom UMG account screen can call `SignIn`, `RequestSignupChallenge`, or `SignUp` on the subsystem, and use distinct request IDs to update its own state. The optional `PlyryAccountWidget` wraps this flow in a local sample interface.
