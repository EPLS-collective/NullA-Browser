# Installing NullA Browser

This guide is for people who just want to run NullA Browser. If you want to build it from source instead, see [BUILDING.md](BUILDING.md).

---

## Windows

1. Go to the [itch.io page](https://epls.itch.io/nulla-browser).
2. Download `NullA Setup.exe`.
3. Run it and follow the installer.

(If you'd rather get it straight from the repository instead of itch.io, it's also kept at `installer/dist/NullA Setup.exe`.)

`NullA Setup.exe` is a small online installer: it always downloads and installs whatever the latest version is at the time you run it, so you never need to check back for a newer installer, the same `NullA Setup.exe` file keeps working correctly for every future release. An internet connection is required while it runs.

If the [Visual C++ Redistributable](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist) isn't already on your system, the installer installs it automatically in the background , no extra steps needed on your part.

That's it, future updates can also be installed directly from inside the browser (Settings -> Updates), so re-running the installer isn't necessary either.

---

## Linux

Install with the script from this repository:

```sh
curl -fsSL https://raw.githubusercontent.com/EPLS-collective/NullA-Browser/main/installer/install-linux.sh | bash
```

This downloads the latest Linux build, installs it into `~/.local/share/NullA`, links a `nulla` command into `~/.local/bin` and adds a menu entry with the icon. Re-running it updates an existing install; your browsing data is never touched.

If you'd rather inspect the script before running it:

```sh
curl -fsSL -o install-linux.sh https://raw.githubusercontent.com/EPLS-collective/NullA-Browser/main/installer/install-linux.sh
bash install-linux.sh
```

Make sure `~/.local/bin` is in your `PATH`, then launch it with:

```sh
nulla
```

---

## Updating

However you installed NullA Browser, once it's running you don't need to repeat these steps for future versions, open **Settings -> Updates** and use the in-app updater.

---

## Uninstalling

### Windows

Use **Settings -> Apps -> Installed apps** (or the old **Control Panel -> Programs and Features**), find NullA Browser, and click **Uninstall**.

### Linux

Run the matching uninstaller (download it first to inspect):

```sh
curl -fsSL -o uninstall-linux.sh https://raw.githubusercontent.com/EPLS-collective/NullA-Browser/main/installer/uninstall-linux.sh
bash uninstall-linux.sh
```

That removes the binary, the symlink, and the menu entry. NullA also keeps your settings and browsing data (cookies, bookmarks, cache, extensions, etc.) separately, in `~/.config/EPLS/` and `~/.local/share/EPLS/`. Pass `--all` to the script to remove those two as well if you want a completely clean uninstall.

---

## Questions?

Open an issue with the `question` tag.
