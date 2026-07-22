#!/bin/bash
set -euo pipefail

REPO="EPLS-collective/NullA-Browser"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="$SCRIPT_DIR/dist"
EXTRACT_DIR="$DIST_DIR/NullA"
ZIP_PATH="$DIST_DIR/NullA-Windows.zip"

VERSION=""
ISCC_PATH="$HOME/.wine/drive_c/Program Files (x86)/Inno Setup 6/ISCC.exe"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --iscc-path)
            ISCC_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

command -v curl >/dev/null || { echo "curl is required but not installed."; exit 1; }
command -v jq >/dev/null || { echo "jq is required but not installed (e.g. sudo zypper install jq)."; exit 1; }
command -v unzip >/dev/null || { echo "unzip is required but not installed."; exit 1; }
command -v wine >/dev/null || { echo "wine is required but not installed."; exit 1; }

if [ -z "$VERSION" ]; then
    echo "No version specified, using the latest GitHub release..."
    RELEASE_URL="https://api.github.com/repos/$REPO/releases/latest"
else
    echo "Using specified version: $VERSION"
    RELEASE_URL="https://api.github.com/repos/$REPO/releases/tags/v$VERSION"
fi

RELEASE_JSON=$(curl -sL -H "User-Agent: NullA-Installer-Builder" "$RELEASE_URL")
TAG=$(echo "$RELEASE_JSON" | jq -r '.tag_name' | sed 's/^v//')

if [ -z "$TAG" ] || [ "$TAG" = "null" ]; then
    echo "Could not resolve a release. Response was:"
    echo "$RELEASE_JSON"
    exit 1
fi
echo "Release found: v$TAG"

DOWNLOAD_URL=$(echo "$RELEASE_JSON" | jq -r '.assets[] | select(.name | test("Windows.*\\.zip$")) | .browser_download_url' | head -n1)

if [ -z "$DOWNLOAD_URL" ] || [ "$DOWNLOAD_URL" = "null" ]; then
    echo "No NullA-Windows.zip asset found in this release."
    exit 1
fi

rm -rf "$EXTRACT_DIR"
mkdir -p "$DIST_DIR"

echo "Downloading: $DOWNLOAD_URL"
curl -sL -o "$ZIP_PATH" "$DOWNLOAD_URL"

echo "Extracting..."
unzip -q -o "$ZIP_PATH" -d "$DIST_DIR"

echo "Compiling Setup.exe (v$TAG)..."
wine "$ISCC_PATH" "/DMyAppVersion=$TAG" "$(winepath -w "$SCRIPT_DIR/NullA.iss")"

echo "Done! Output: dist/NullA Setup.exe"
