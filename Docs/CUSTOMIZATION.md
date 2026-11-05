# Customize the in-game UI

Plyry's optional account panel is a normal Unreal `UUserWidget`. It is built with UMG widgets and makes API calls through `UPlyryApiClientSubsystem`. It does not create a Web Browser widget or load plyry.com.

## Theme asset

Create a Data Asset based on `PlyryAccountTheme` and set it on your derived Widget Blueprint. The asset holds:

- Panel, surface, accent, text, and muted colors.
- Header/body fonts.
- Full UMG button and editable-text-box styles. Enable the matching override property to replace the engine style; with overrides off, the palette tints the native controls.
- Panel padding and width.

The widget also exposes editable labels and hint text. Use project fonts and brushes in the style structs to match your game's UI.

## Keep the supplied layout

1. Create a Widget Blueprint derived from `PlyryAccountWidget`.
2. Set its `Theme` and customize labels in Class Defaults.
3. Create an instance with `Create Widget` and add it to the viewport.
4. Bind `OnStatusChanged`, `OnSignupQuestionChanged`, `OnSignupChallengeReceived`, and `OnAccountDataChanged` if the game needs custom transitions or analytics.

## Replace the layout

Create a derived Widget Blueprint with your own designer tree. Set `Build Default Layout` to false so the native widget leaves that tree intact. Use these Blueprint-callable functions from your controls:

- `SignInWithCredentials(Email, Password, Scopes)`
- `RequestSignupChallengeForEmail(Email)`
- `SignUpWithChallenge(Email, Password, Challenge, Answer, Scopes)`
- `RefreshAccount()`
- `SignOut()`

The challenge response event provides the server's challenge JSON so a custom form can hold the short-lived challenge value and display its question. Alternatively, use the API subsystem's authentication methods directly. Subscribe to `OnRequestCompleted` and route the unique request IDs your UI uses.

The sample widget's default sign-in/sign-up requests `account:read` and `wallet:read`. A custom screen can pass its own least-privilege scope list.

## Use no account panel

The client is independent of the widget. Add the `PlyryUnreal` module dependency and use the Game Instance subsystem from game code or a custom UMG screen. There is no required Plyry front-end page or browser sign-in step.
