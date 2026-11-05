# Security notes

## User sessions

- Access and refresh tokens stay in private subsystem memory. They are not Blueprint properties, session-snapshot fields, or authentication response bodies.
- Auth helper responses remove access-token and refresh-token fields from both the text body and byte array before broadcasting. The account snapshot also strips those fields from returned user data.
- By default, the plugin does not save refresh tokens to disk. Sessions last for the running game process.
- To restore a session after restart, implement `UPlyryCredentialStore` with the target platform's secure credential facility, assign the object with `SetCredentialStore`, then call `RestoreSession`. Do not store refresh tokens in ordinary config, logs, SaveGame data, or source control.
- Sign-out clears local credentials immediately and asks the API to revoke the refresh token.
- Changing the API base URL clears the active session, saved refresh token, and configured API key so credentials are not reused with a different API root.

## API keys

`SetApiKey` supports trusted server-side applications. Do not put a Plyry API key in a packaged Unreal game, Blueprint, config file, or client-side website: players can extract client binaries and assets. For games running on a player's device, sign in with the user's account and request the scopes the game needs.

## Transport and web apps

The client accepts HTTPS API roots. Plain HTTP is allowed for `localhost` and `127.0.0.1` development only. Browser-hosted sites still need the API's allowed-origin/CORS configuration and linked-app flow. Unreal native HTTP requests do not require a browser and do not load the Plyry account UI.

## Payment providers

The plugin can send wallet top-up requests and return the provider configuration/response. A payment processor may return an action URL that your game must handle according to its platform's payment rules. This plugin does not embed Plyry pages or the provider checkout page. Do not collect card data through this plugin.
