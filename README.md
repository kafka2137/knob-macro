# Kafka Keyboard Macros

A browser editor and custom firmware for the **Keychron Q8 ISO encoder**. Save text, shortcuts, and key sequences to the keyboard over USB without reflashing for every macro change.

The editor is a static website that runs on GitHub Pages or localhost, with no application server or frontend dependencies.

## Features

- **30 independent macros:** 15 on FN3 and 15 on FN4, each with its own values and encoder assignments.
- **Text macros:** UTF-8 text, including Polish characters, other writing systems, combining marks, and emoji. Windows Unicode input uses WinCompose.
- **Normal macros:** key taps, presses, releases, chords, and pauses. Use the action editor or record keys in the browser.
- **Encoder selection:** clockwise advances and counterclockwise goes back through 15 steps. Assign any macro from the same layer, or leave a step unassigned.
- **Knob press:** cancel text typing; restart a normal macro immediately, bypassing rotation debounce.
- **Interruptible playback:** another selection cancels the previous macro and releases keys owned by it, even during a long pause.
- **Backups and drafts:** import/export JSON. Browser drafts and keyboard storage remain separate.

**FN3 and FN4 mean numeric layers 3 and 4 in VIA/Keychron Launcher.** The editor selector chooses the library to edit. Your keyboard's Fn bindings activate layers; the site does not activate them.

## Get started

1. Download `keychron_q8_iso_encoder_kafka.bin` from this repository's **Releases** and flash it once through your usual QMK Toolbox/DFU workflow. This binary is for the Q8 ISO encoder only.
2. For Unicode on Windows, install [WinCompose](https://github.com/samhocevar/wincompose), enable Unicode input, and keep **Right Alt** as the Compose key. Polish (Programmers) or US matches the ASCII punctuation lookup.
3. Open the GitHub Pages site in a WebHID-capable Chromium browser such as Chrome or Edge. Close VIA/Keychron Launcher before connecting.
4. Click **Connect keyboard**, read its library or edit drafts, choose FN3/FN4, then **Save library**.
5. Activate numeric layer 3 or 4 through your keyboard bindings and turn or press the knob.

Future macro edits use USB Raw HID and require **no flash mode**. Changing compiled firmware settings still requires a firmware update.

**Export a backup before upgrading firmware.** Moving from the older shared-macro firmware changes EEPROM layout. The editor supports read/export-only access to older UTF-8 firmware. Import and save your backup after upgrading. Shared definitions migrate into separate copies for both layers, preserving their assignments. Earlier edits overwritten in the old shared pool cannot be recovered.

## Playback

An encoder-selected text macro sends Ctrl+A, Delete, then types its text, replacing the active input. Focus the target field before turning the knob. Assigned VIA macro keys insert text without the replacement prefix.

Normal macros replay key actions without selecting/deleting text. Rotation waits **200 ms after the latest selection** before starting a normal macro. Text does not use that debounce. Knob press and assigned macro keys start normal macros without the 200 ms wait. Pressing before the first encoder selection or on an unassigned step cancels without starting a macro. Outside macro layers, the knob retains its existing binding.

Playback uses QMK deferred execution. Cancelling text stops future typing but does not undo characters already sent. Unfinished WinCompose sequences are cleared with Escape. Restarting a normal macro releases owned keys before replaying from action one; physically held keys/modifiers are not claimed by it.

Each macro allows **1,023 encoded bytes**, not 1,023 characters. Normal action headers and multibyte Unicode count toward that limit. Delays support 0–65,535 ms. Normal actions use 8-bit QMK keycodes; arbitrary 16-bit quantum/layer operations are unsupported.

## Run locally

```sh
python3 -m http.server 8000 --directory dist
```

Open `http://localhost:8000`. WebHID requires HTTPS or localhost. On Linux, your user needs access to the keyboard's hidraw device. Browser-reserved shortcuts may need manual entry through the action picker.

Checks require Node.js and a C compiler:

```sh
bash scripts/check.sh
```

Checks cover JavaScript syntax, 15 protocol tests, and the actual firmware player with debounce settings 200 ms, 350 ms, and 0. The ARM firmware build passed in the development environment. Hardware playback and browser device interaction are not covered by automated tests.

## Deploy to GitHub Pages

Create an empty **public** GitHub repository. From this checkout, replace `OWNER/REPO` with your account and repository name:

```sh
git remote add origin https://github.com/OWNER/REPO.git
git push -u origin main
```

In GitHub, open **Settings → Pages → Build and deployment → Source** and choose **GitHub Actions**. Run **Deploy GitHub Pages** from the Actions tab, or push another commit to `main`. The workflow checks the project, prepares static files, and deploys them. See [GitHub's custom Pages workflow documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

The usual URL is `https://OWNER.github.io/REPO/`. Relative asset paths support the repository subdirectory.

### Where to change addresses later

- **GitHub remote:** `git remote set-url origin https://github.com/NEW_OWNER/NEW_REPO.git`.
- **Firmware download:** no source edit needed. `scripts/prepare-pages.mjs` uses `GITHUB_REPOSITORY` from Actions to link to the current repository's latest release. A rename, transfer, or fork picks up the new address on the next deployment.
- **Custom domain:** configure it in GitHub **Settings → Pages**. The editor has no fixed Pages hostname.
- **Local deployment preview:** `node scripts/prepare-pages.mjs OWNER/REPO`, then serve `_site/`.

The firmware link works once the first stable release contains the expected asset.

## Firmware releases

Compiled binaries are **not committed to Git** or bundled with the website. Source is in `firmware/kafka/`. `.gitignore` excludes firmware blobs and the local `release-assets/` directory. The initial prebuilt binary is in that ignored directory in this prepared checkout; fresh clones need a release download or a local QMK build.

After pushing source, install [GitHub CLI](https://cli.github.com/) and authenticate:

```sh
gh auth login
bash scripts/release-firmware.sh v1.0.0 release-assets/keychron_q8_iso_encoder_kafka.bin
```

The script checks the project, requires committed source, creates a release targeting the current source commit, and uploads `keychron_q8_iso_encoder_kafka.bin` and `SHA256SUMS`.

You can also create a release in GitHub's web interface. Tag the source commit used to build the binary and attach both files. Keep the binary's filename unchanged: the website uses `/releases/latest/download/keychron_q8_iso_encoder_kafka.bin`. Publish a stable release so it is available under `latest`.

Verify a download with `sha256sum -c SHA256SUMS` from the directory containing both files. The checksum checks file integrity; provenance still depends on the builder and source commit.

See [firmware/README.md](firmware/README.md) for QMK installation, settings, and the storage protocol.

## Privacy

Macro contents travel directly between browser and keyboard over WebHID. The editor does not upload them to an application server. GitHub hosts the static site and firmware downloads. Avoid committing personal JSON backups.

Saving verifies the full library before enabling playback. An interrupted save leaves playback disabled until a successful retry. Keep backups before resetting EEPROM or changing firmware. JSON backups use `kafka-macros-v4` and include both layers.

## License

See [LICENSE](LICENSE). Existing Keychron/QMK copyright notices are retained in the firmware source.
