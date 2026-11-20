# Notify when a newer release is available — implementation spec

Developer spec for a **fail-closed**, **non-modal** update hint on System Select. Keep a copy in both repositories and edit them together:

- gssquared: `Docs/NotifyUpdate.md`
- gssquared-web: `docs/NotifyUpdate.md`

User-facing today: **Docs → Check For Updates** already opens the HTML updates page in the system browser. This spec adds a background check and a button on the chooser. It does **not** install anything.

## Goal

If a desktop build is behind the latest **published** GitHub release, show one extra button under **+ New / Launch / Edit...**:

**Click to get new version**

Clicking it opens the same gssquared.net HTML page the menu item already opens. The user downloads and installs themselves.

If anything is uncertain (offline, timeout, bad body, current, newer-than-release, web build), show nothing extra. No error UI. No modal.

## What already exists

| Piece | Where |
|---|---|
| Menu item opens `https://gssquared.net/updates/{version}/{git-hash}/{platform}` | gssquared `src/util/CheckForUpdates.cpp` |
| Pretty URL → `updates.php?version=&commit=&platform=` | gssquared-web `public/.htaccess`, `public/router.php` |
| HTML page compares visitor build to GitHub latest (15-minute cache) | gssquared-web `public/updates.php`, `includes/github_release.php` |
| Status values: `available`, `current`, `newer`, `latest`, `unknown` | `updates.php` |
| Desktop HTTPS GET (to a **file**, pack-download timeouts) | gssquared `src/platform-specific/HttpsGet.*` |
| TOML parser (toml++); **no** JSON parser in the app | gssquared `src/util/toml.hpp` |
| System Select action bar | gssquared `src/ui/SelectSystem.cpp` |
| Background thread + cancel pattern | gssquared `src/ui/PackFetchModal.cpp` |

Version strings are `MAJOR.MINOR.{git rev-list --count HEAD}` (for example `1.0.4523`) plus a short git hash. The PHP already compares those triples and treats a visitor **ahead** of the last published asset as `newer` (dev build), not `available`.

The app must **not** talk to GitHub. The site already does that, once per 15 minutes for everyone.

## User flow

```
start GSSquared (desktop, System Select actually shown)
        │
        ▼
  background GET  (short timeout)
        │
   ┌────┴─────────────────────────┐
 fail / current / newer / unknown │  status = available
   │                              │
   ▼                              ▼
 no extra UI            show button under + New / Launch / Edit...
                                  │
                                  ▼
                         click → SDL_OpenURL(HTML updates page)
```

- The button is only on System Select. It is never drawn over a running Apple II.
- **Docs → Check For Updates** stays as-is (always opens the HTML page, even when current).
- No skip, snooze, or “don’t check automatically” in v1. A quiet button is enough.
- Hover hint (same footer line as the other action buttons): e.g. “A newer GSSquared is available.”

## Fail closed

Show the button only when **all** of these are true:

1. Desktop build (not Emscripten / `gssquared.net/live`).
2. System Select is the screen the user is looking at (not a `-p` / config-path auto-launch that only constructs `SelectSystem` in order to destroy it).
3. The GET returned HTTP 200.
4. The body parsed as TOML.
5. `status` is the string `available`.

Otherwise: no button, no log spam, no retry loop. Treat as “not now.”

Never block startup on the network. Never block a machine launch for more than the short cancel/timeout below.

## URL contract

Same path the menu already uses, plus a query flag the rewrite already forwards (`[L,QSA]`):

```
https://gssquared.net/updates/{VERSION_STRING}/{GIT_HASH}/{platform}?format=toml
```

`platform` is `macos`, `windows`, `linux`, or `other` — same helper as `CheckForUpdates.cpp`.

Browser visits **without** `format=toml` stay HTML. The app GET **with** `format=toml` must not receive the HTML page.

Clicking the button (and the menu item) must open the HTML URL, **without** `?format=toml`.

Suggested `User-Agent`: `GSSquared/{VERSION_STRING}` so server logs can tell an app ping from a browser.

### TOML body (v1)

Small, boring, quoted strings only. No tables, no arrays, no news copy.

```toml
status = "available"
latest = "1.0.4523"
```

| Key | Required | Meaning |
|---|---|---|
| `status` | yes | `available` — visitor is behind the latest published build. `current` — matches. `newer` — visitor is ahead (dev build). `unknown` — site could not decide (GitHub down, bad visitor version, etc.). |
| `latest` | no | Latest published build string, when known. The client does not need it to decide; it may use it later. Do not compare versions in C++. |

The PHP already computes `status`. **Reuse that.** Do not reimplement `parse_build_version` in the app.

If `status` is missing, not a string, or not `available`, fail closed.

`Content-Type: text/plain; charset=utf-8` is enough. Cap the body at a few kilobytes; a huge response is a failure.

## Website (gssquared-web)

Land this first so the client has a real endpoint.

In `public/updates.php`:

- If `$_GET['format'] === 'toml'`: do **not** call `render_header` / `render_footer`. Send the TOML body and exit.
- Use the same `$status` / `$latest_build` the HTML page already computed.
- Map HTML-only `latest` (unpersonalized `/updates`) to TOML `unknown` if you like; the app always sends version/hash/platform, so it will be personalized.
- On GitHub failure (`$status === 'unknown'`), still return 200 + `status = "unknown"` rather than a 5xx. The client fails closed either way; a 200 TOML is easier to test.

Also handle the three-segment path in `public/router.php` (local `php -S`) the same as Apache: keep `format` from the query string.

No new rewrite rule is required if `?format=toml` is used.

Do not introduce a JSON API. The marketing site may keep using JSON **internally** for the GitHub cache (`includes/github_release.php`); that stays server-side.

## App (gssquared)

Desktop only (`#ifndef __EMSCRIPTEN__`).

### When to start the GET

**Not** in the `SelectSystem` constructor. `SDL_AppInit` always constructs `SelectSystem`, then `-p` / a config path immediately `transition_to_emulation()`, which deletes it. A constructor GET would stall every smoke test on `SDL_WaitThread`.

Start from `SelectSystem::update()` (or the System Select iterate path) the first time that object is actually ticked. One GET per `SelectSystem` instance is enough. Returning to the chooser after **File → Close Emulation** constructs a new `SelectSystem` and may check again; that is fine (the site’s GitHub cache is 15 minutes).

### Threading and timeouts

- `SDL_CreateThread`, same cancel/`done` pattern as `PackFetchModal`.
- Poll `done` in `SelectSystem::update()`. When it completes, `mark_dirty()` so the button can appear. Do not touch UI from the worker thread.
- Destructor: set cancel, `SDL_WaitThread`.
- **Short timeouts**, not pack-download timeouts. Windows `https_get_to_file` is 10s / 30s / 60s; Linux libcurl currently has **no** timeout. This check needs a few seconds total (on the order of 3–8s). Offline DNS must not delay launching a machine.
- Body is tiny: add a small in-memory GET (`https_get_to_string` or equivalent) rather than streaming a pack file. Reusing `https_get_to_file` into PrefPath is acceptable only if timeouts are still short and the temp file is deleted.

Parse with `toml::parse` on the string. Catch `toml::parse_error` and fail closed.

### The button

Do **not** add a fourth child to the existing `action_con_` (+ New / Launch / Edit...). That container lays out a grid using the **largest** tile as the cell size. A wide update button would stack the three action buttons vertically.

Use a **separate** `Button_t` under the action bar, hidden until `status == "available"`:

- Label: `Click to get new version`
- Width: about the full action bar (~420px), height like the others (36px)
- Same `ActionStyle` as New / Launch / Edit
- Click: `openCheckForUpdates()` (HTML URL, no query flag)
- Leave room so it does not sit on the footer hint at `description_y` (880 in design space)

Hidden means `set_visible(false)` and no hit-testing, not a disabled-looking control.

## Out of scope (v1)

- In-app download or replace-the-running-binary
- Sparkle / WinSparkle / AppImage self-update
- JSON in the app
- Modals, toasts, menu badges, interrupting emulation
- Skip this version / snooze / opt-out checkbox
- News / “cool new stuff” in the same control (the HTML page can keep its news section)
- Unique install IDs or extra telemetry beyond version, hash, and platform already in the URL
- Checking from the Emscripten build

## Privacy

The URL already identifies this build (version, git hash, platform) when the user clicks **Check For Updates**. A background GET sends the same data without a click. No machine config, disk names, or install UUID. Document that on the site if you mention automatic checks in help/privacy copy later.

## Landing order

1. gssquared-web: TOML branch in `updates.php` (and `router.php` for local preview). Curl it.
2. gssquared: background GET, parse, hidden-until-needed button.

The menu item must keep working if step 2 ships first; it only opens HTML.

## Manual checks

**Site**

```bash
curl -sS "https://gssquared.net/updates/0.0.1/deadbeef/macos?format=toml"
# expect: status = "available"  (0.0.1 is behind any real release)

curl -sS "https://gssquared.net/updates/999.0.0/deadbeef/macos?format=toml"
# expect: status = "newer"

curl -sS -A "Mozilla/5.0" "https://gssquared.net/updates/0.0.1/deadbeef/macos"
# expect: HTML, not TOML
```

**App**

- Offline at launch: chooser appears immediately; no extra button.
- Online, behind latest: button appears under the action bar without a modal; click opens the HTML updates page.
- Online, current or a git build ahead of the last tag: no extra button.
- `-p 3` / config-path auto-launch: no multi-second stall in `SDL_AppInit`.
- Web build: no GET, no button.
