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

Download, extract, and link the browser into your `PATH`:

```sh
curl -fsSL -o NullA-Linux.tar.gz "$(curl -fsSL https://api.github.com/repos/EPLS-collective/NullA-Browser/releases/latest | grep browser_download_url | grep Linux | cut -d '"' -f4)"
tar -xzf NullA-Linux.tar.gz
mkdir -p ~/.local/share
mv NullA ~/.local/share/NullA
mkdir -p ~/.local/bin
ln -sf ~/.local/share/NullA/NullA ~/.local/bin/nulla
mkdir -p ~/.local/share/applications
curl -fsSL "https://raw.githubusercontent.com/EPLS-collective/NullA-Browser/main/resources/nulla.desktop" | sed "s|HOME_PLACEHOLDER|$HOME|g" > ~/.local/share/applications/nulla.desktop
curl -fsSL -o ~/.local/share/NullA/nulla_icon.png "https://raw.githubusercontent.com/EPLS-collective/NullA-Browser/main/resources/nulla_icon.png"
update-desktop-database ~/.local/share/applications 2>/dev/null || true
```

This also adds NullA to your app launcher/menu using the icon and `.desktop` file kept in this repository. Make sure `~/.local/bin` is in your `PATH`, then launch it with:

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

```sh
rm -rf ~/.local/share/NullA
rm -f ~/.local/bin/nulla
rm -f ~/.local/share/applications/nulla.desktop
update-desktop-database ~/.local/share/applications 2>/dev/null || true
```

That removes the binary, the symlink, and the menu entry. NullA also keeps your settings and browsing data (cookies, bookmarks, cache, extensions, etc.) separately, in `~/.config/EPLS/` and `~/.local/share/EPLS/`, remove those two as well if you want a completely clean uninstall.

---

## Questions?

Open an issue with the `question` tag.
