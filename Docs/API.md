# API reference

Plyry's live OpenAPI document at [https://plyry.com/v1/openapi.json](https://plyry.com/v1/openapi.json) defines the wire contract. This plugin exposes generic request methods and convenience methods for the routes below.

## Request methods

| Method | Use |
| --- | --- |
| `RequestJson` | Send a relative API route with JSON or no body. |
| `RequestBytes` | Send or receive raw bytes, including media downloads. |
| `RequestMultipart` | Send named text fields and file parts with a generated multipart boundary. |

Use paths relative to the API root, such as `/models` or `/generations/{hash}`; don't include `/v1` in the path because the default root already ends in `/v1`. Set `bAuthenticated` to false only for public requests. The client adds bearer auth for authenticated routes and blocks caller-supplied overrides of protected headers.

Each completion is broadcast through `OnRequestCompleted`. Give each call a distinct `RequestId`. Use `Body` for JSON/text, `Bytes` for binary content, `ContentType` for representation, and `Headers` for response metadata. The response omits `Set-Cookie` and authorization headers.

## Authentication

| Client method | Route |
| --- | --- |
| `RequestSignupChallenge` | `POST /auth/signup/challenges` |
| `SignIn` | `POST /auth/login` |
| `SignUp` | `POST /auth/signup` |
| `RefreshSession` / automatic retry | `POST /auth/refresh` |
| `SignOut` | `POST /auth/revoke` |

The auth helpers consume token responses internally. Access and refresh tokens are not included in public events or session snapshots. Configure `SetLinkedAppName` before sign-in to create a named linked-app entry.

## Supported route groups

| Route | Plugin methods |
| --- | --- |
| `GET /openapi.json` | `GetOpenApiSpecification` |
| `GET /models` | `GetModels` |
| `GET /me` | `GetMe` |
| `GET /wallet`, `GET /wallet/ledger` | `GetWallet`, `GetWalletLedger` |
| `GET /payments/config` | `GetPaymentConfig` |
| `POST /quotes` | `QuoteGeneration` |
| `GET /generations`, `GET /generations/{hash}` | `ListGenerations`, `GetGeneration` |
| `POST /generations` | `CreateGeneration` (multipart) |
| `GET /media/{hash}`, `GET /media/{hash}/scene` | `DownloadGenerationMedia` |
| `POST /generations/{hash}/cache` | `AcknowledgeGenerationCache` |
| `POST /wallet/topups` | `CreateWalletTopup` |
| `GET /wallet/topups/requests/{requestKey}` | `GetWalletTopupStatus` |
| `POST /wallet/topups/reconcile` | `ReconcileWalletTopup` |
| `GET /conversions/plant3d-to-revit/price` | `GetPlant3DToRevitPrice` |
| `POST /conversions/plant3d-to-revit/jobs` | `CreatePlant3DToRevitJob` (multipart) |
| `GET /conversions/plant3d-to-revit/jobs/{hash}` | `GetPlant3DToRevitJob` |
| `GET /conversions/plant3d-to-revit/jobs/{hash}/buttons` | `SearchPlant3DToRevitButtons` |
| `POST /conversions/plant3d-to-revit/jobs/{hash}/mapping` | `RevisePlant3DToRevitMapping` |
| `GET /conversions/plant3d-to-revit/jobs/{hash}/download` | `DownloadPlant3DToRevitMapping` |

The generic request methods also cover routes added later to v1, including payment return handling. The plugin doesn't implement a media-generation workflow or an engine-specific file importer.

## Uploads and retries

Build multipart text fields with `FPlyryFormField` and file parts with `FPlyryFormFile`. The API contract defines field names and upload limits; see the OpenAPI schema for the selected route. Generation references use the API's field conventions, including repeated fields when required.

Provide an idempotency key on create/top-up calls when supported. If the access token expires, the client refreshes once and retries the original request with the same key. When your app explicitly retries a create operation after a lost connection, reuse the same key only for the same logical operation.

## Scopes

The server returns the granted scopes after sign-in. Current examples include `account:read`, `wallet:read`, `wallet:topup`, `generation:create`, `generation:read`, `conversion:create`, and `conversion:read`. Request only what the game needs and handle a permission response by explaining the missing capability to the user.
