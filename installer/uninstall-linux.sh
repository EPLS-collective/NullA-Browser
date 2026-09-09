#!/bin/bash
# Removes a ./install-linux.sh install: binary, symlink and menu entry.
# Browsing data (~/.config/EPLS and ~/.local/share/EPLS) is kept unless --all is given.

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

purge=false
[ "${1:-}" = "--all" ] && purge=true

apps_dir="$HOME/.local/share/applications"

echo -e "${YELLOW}➜ Removing binary and symlink...${NC}"
rm -rf "$HOME/.local/share/NullA"
rm -f "$HOME/.local/bin/nulla"
rm -f "$apps_dir/nulla.desktop"
update-desktop-database "$apps_dir" 2>/dev/null || true

if $purge; then
    echo -e "${YELLOW}➜ Removing browsing data...${NC}"
    rm -rf "$HOME/.config/EPLS"
    rm -rf "$HOME/.local/share/EPLS"
fi

echo -e "${GREEN}✓ Uninstalled${NC}"
if ! $purge; then
    echo -e "${YELLOW}   (browsing data kept; re-run with --all to purge it)${NC}"
fi