#!/bin/bash
# Installs the latest NullA Browser Linux release into ~/.local and adds a
# menu entry. Re-running updates an existing install; browsing data is never touched.

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

repo="EPLS-collective/NullA-Browser"
app_dir="$HOME/.local/share/NullA"
bin_dir="$HOME/.local/bin"
apps_dir="$HOME/.local/share/applications"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo -e "${YELLOW}➜ Fetching latest Linux build info...${NC}"
tarball="$(curl -fsSL "https://api.github.com/repos/${repo}/releases/latest" \
    | grep browser_download_url | grep Linux | cut -d '"' -f4)"
if [ -z "$tarball" ]; then
    echo -e "${RED}✗ No Linux build found in the latest release${NC}"
    exit 1
fi

echo -e "${YELLOW}➜ Downloading ${tarball##*/}...${NC}"
curl -fsSL -o "$tmp/NullA-Linux.tar.gz" "$tarball"

echo -e "${YELLOW}➜ Installing to ${app_dir}...${NC}"
mkdir -p "$(dirname "$app_dir")"
rm -rf "$app_dir"
tar -xzf "$tmp/NullA-Linux.tar.gz" -C "$tmp"
mv -f "$tmp/NullA" "$app_dir"

if [ ! -x "$app_dir/NullA" ] || [ ! -f "$app_dir/qt.conf" ] || [ ! -d "$app_dir/plugins/platforms" ]; then
    echo -e "${RED}✗ Downloaded package is missing Qt runtime files${NC}"
    exit 1
fi

echo -e "${YELLOW}➜ Linking launch command...${NC}"
mkdir -p "$bin_dir"
rm -f "$bin_dir/nulla"
cat > "$bin_dir/nulla" <<EOF
#!/bin/sh
LD_LIBRARY_PATH="$app_dir\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
if [ -z "\$QT_QPA_PLATFORM" ] && [ "\$XDG_SESSION_TYPE" = "wayland" ] && [ ! -f "$app_dir/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so" ]; then
    QT_QPA_PLATFORM=xcb
    export QT_QPA_PLATFORM
fi
exec "$app_dir/NullA" "\$@"
EOF
chmod +x "$bin_dir/nulla"

rm -f "$bin_dir/qt.conf" "$bin_dir/NullA" "$bin_dir/QtWebEngineProcess"
rm -rf "$bin_dir/plugins" "$bin_dir/resources" "$bin_dir/translations"

echo -e "${YELLOW}➜ Adding app menu entry...${NC}"
mkdir -p "$apps_dir"
cat > "$apps_dir/nulla.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=NullA Browser
GenericName=Web Browser
Comment=Fast for freedom
Exec=$bin_dir/nulla %U
Icon=$app_dir/nulla_icon.png
Terminal=false
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
StartupWMClass=NullA
EOF
curl -fsSL -o "$app_dir/nulla_icon.png" \
    "https://raw.githubusercontent.com/${repo}/main/resources/icons/nulla_icon.png" || true
update-desktop-database "$apps_dir" 2>/dev/null || true

echo ""
echo -e "${GREEN}✓ Installed. Make sure ${bin_dir} is in your PATH, then run:${NC}"
echo -e "${GREEN}  nulla${NC}"