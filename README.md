# Plyry for Unreal Engine

A standalone Unreal Engine 5 runtime plugin for the Plyry API. It calls Plyry over native HTTP requests and includes an optional account panel built with UMG. The plugin does not open or embed plyry.com pages.

## Features

- Direct asynchronous JSON, byte, and multipart requests.
- Sign-in, sign-up challenge, account linking, refresh, restore, and sign-out helpers.
- Automatic access-token refresh and one retry after an authenticated request returns HTTP 401.
- API helpers for models, wallet, generation, media, payments, and Plant3D-to-Revit conversion.
- Binary response bytes and response metadata for media downloads.
- Optional local UMG sign-in/account panel, editable copy, and theme Data Asset.
- Custom widgets can use the client subsystem directly without the supplied panel.
- Refresh-token persistence is opt-in through a host-provided credential store.

## Requirements

- Unreal Engine 5.x with the HTTP and UMG runtime modules.
- A game module that can depend on the `PlyryUnreal` module.
- Network access to the Plyry API at `https://plyry.com/v1`.

## Install

Copy this repository into the game project's `Plugins/Plyry` directory, enable the **Plyry** plugin, and add `PlyryUnreal` to the game module's dependencies:

```csharp
PublicDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "PlyryUnreal"
});
```

Set the API root in `Config/DefaultGame.ini` if you use another environment:

```ini
[/Script/PlyryUnreal.PlyryApiClientSubsystem]
ApiBaseUrl=https://plyry.com/v1
```

The default API root is already `https://plyry.com/v1`. Local HTTP is accepted only for `localhost` and `127.0.0.1` development servers. Changing the root at runtime clears the active session and credentials before sending future requests.

## Make a request

Get the shared client from the game instance. Responses arrive on `OnRequestCompleted`; use a unique request ID to route results from concurrent calls.

```cpp
UPlyryApiClientSubsystem* Plyry =
    GetGameInstance()->GetSubsystem<UPlyryApiClientSubsystem>();

Plyry->OnRequestCompleted.AddUniqueDynamic(
    this, &UMyWidget::HandlePlyryResponse);

Plyry->GetModels(TEXT("models.initial"));
```

The Blueprint API offers the same subsystem methods. For a route without a convenience method, use `RequestJson`, `RequestBytes`, or `RequestMultipart`. The response includes an HTTP status, error text, text body, content type, safe response headers, and raw bytes. JSON and text responses populate `Body`; binary downloads populate `Bytes`.

The calls in this plugin use the stable Public API v1 paths. The API's current OpenAPI document is at [plyry.com/v1/openapi.json](https://plyry.com/v1/openapi.json). See [Docs/API.md](Docs/API.md) for route coverage and request patterns.

## Sign in and scopes

Use the client methods `RequestSignupChallenge`, `SignIn`, `SignUp`, `RefreshSession`, `RestoreSession`, and `SignOut`. Sign-in links the account to the app name sent in the auth request. Call `SetLinkedAppName` before sign-in to set the label shown to users.

Request only the scopes the game uses. The optional sample panel requests `account:read` and `wallet:read`. Pass additional scopes such as `generation:create`, `generation:read`, `wallet:topup`, `conversion:create`, or `conversion:read` in the sign-in/signup call as needed. The session snapshot reports granted scopes without exposing tokens.

See [Docs/SECURITY.md](Docs/SECURITY.md) before selecting a credential store or using API keys.

## Optional local account panel

Create a Widget Blueprint derived from `PlyryAccountWidget`, then create a widget from that Blueprint and add it to the viewport. It builds local UMG controls for sign-in, account creation, account refresh, and sign-out. It uses the client subsystem; it never creates a browser view.

Create a Data Asset based on `PlyryAccountTheme` to customize the panel colors, fonts, input/button styles, padding, and width. The widget's copy is editable in the Blueprint defaults. For a fully custom designer layout, set `Build Default Layout` to false, add your own controls, and call the exposed `SignInWithCredentials`, `RequestSignupChallengeForEmail`, `SignUpWithChallenge`, `RefreshAccount`, and `SignOut` functions. Bind the widget's status, question, challenge response, and account-data events to your own presentation.

More detail is in [Docs/CUSTOMIZATION.md](Docs/CUSTOMIZATION.md).

## Limits and behavior

- The request layer returns binary data in memory. Large downloads and multipart uploads consume game-process memory; honor the current API limits and use your own file/stream pipeline for very large payloads.
- This plugin submits payment-top-up requests and reports the API response. Payment-provider authorization and any external checkout are app-owned; the plugin does not embed a Plyry or processor website.
- Browser-hosted website callers use the API's CORS and linked-origin behavior. Unreal's native HTTP requests do not render websites.
- API features and required scopes can evolve. Use the live OpenAPI document as the source of truth.

## License

MIT. See [LICENSE](LICENSE).
