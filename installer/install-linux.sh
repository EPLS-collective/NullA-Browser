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

echo -e "${YELLOW}➜ Linking launch command...${NC}"
mkdir -p "$bin_dir"
ln -sf "$app_dir/NullA" "$bin_dir/nulla"

echo -e "${YELLOW}➜ Adding app menu entry...${NC}"
mkdir -p "$apps_dir"
curl -fsSL "https://raw.githubusercontent.com/${repo}/main/resources/nulla.desktop" \
    | sed "s|HOME_PLACEHOLDER|$HOME|g" > "$apps_dir/nulla.desktop"
curl -fsSL -o "$app_dir/nulla_icon.png" \
    "https://raw.githubusercontent.com/${repo}/main/resources/nulla_icon.png"
update-desktop-database "$apps_dir" 2>/dev/null || true

echo ""
echo -e "${GREEN}✓ Installed. Make sure ${bin_dir} is in your PATH, then run:${NC}"
echo -e "${GREEN}  nulla${NC}"