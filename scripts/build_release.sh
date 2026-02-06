# Usage: ./scripts/build_release.sh [version_arg] [repo_slug]
# If version_arg is "auto" (default), it increments the PATCH version.
# If version_arg is a specific string (e.g., v1.35.0), it sets that version.
# If repo_slug is provided (e.g., "rexackermann/hid-gadget-module"), it updates module.prop/update.json URLs.

set -e

# We assume the script is run from the root of the repository
# If run from scripts/, we need to cd ..
if [ -d "../src" ]; then
    cd ..
fi

MODULE_PROP="module.prop"

# Function to get current version
get_current_version() {
    grep "^version=" "$MODULE_PROP" | cut -d'=' -f2
}

# Function to get current version code
get_current_code() {
    grep "^versionCode=" "$MODULE_PROP" | cut -d'=' -f2
}

# 1. Determine Target Version
TARGET_VER="$1"
REPO_SLUG="$2" # Optional: e.g. "rexackermann/hid-gadget-module"
CURRENT_VER=$(get_current_version)
CURRENT_CODE=$(get_current_code)

if [ -z "$TARGET_VER" ] || [ "$TARGET_VER" = "auto" ] || [ "$TARGET_VER" = "skip" ]; then
    if [ "$TARGET_VER" = "skip" ]; then
        TARGET_VER="$CURRENT_VER"
        TARGET_CODE="$CURRENT_CODE"
    else
        # Auto-bump patch
        ver_clean=${CURRENT_VER#v}
        MAJOR=$(echo $ver_clean | cut -d. -f1)
        MINOR=$(echo $ver_clean | cut -d. -f2)
        PATCH=$(echo $ver_clean | cut -d. -f3)
        NEW_PATCH=$((PATCH + 1))
        TARGET_VER="v$MAJOR.$MINOR.$NEW_PATCH"
        TARGET_CODE=$((CURRENT_CODE + 1))
    fi
else
    # Explicit version set (e.g., v1.35.0)
    if ! echo "$TARGET_VER" | grep -qE "^v[0-9]+\.[0-9]+\.[0-9]+$"; then
        echo "Error: Invalid version format. Use vX.Y.Z"
        exit 1
    fi
    ver_clean=${TARGET_VER#v}
    MAJOR=$(echo $ver_clean | cut -d. -f1)
    MINOR=$(echo $ver_clean | cut -d. -f2)
    PATCH=$(echo $ver_clean | cut -d. -f3)
    TARGET_CODE=$((MAJOR * 10000 + MINOR * 100 + PATCH))
fi

echo ">> Bumping version: $CURRENT_VER -> $TARGET_VER (Code: $CURRENT_CODE -> $TARGET_CODE)"

# 2. Update metadata files
sed -i "s/^version=.*/version=$TARGET_VER/" "$MODULE_PROP"
sed -i "s/^versionCode=.*/versionCode=$TARGET_CODE/" "$MODULE_PROP"

if [ -n "$REPO_SLUG" ]; then
    echo ">> Updating Repository URLs to: $REPO_SLUG"
    sed -i "s|^updateJson=.*|updateJson=https://raw.githubusercontent.com/$REPO_SLUG/main/update.json|" "$MODULE_PROP"
    sed -i "s|\"changelog\": \".*\"|\"changelog\": \"https://github.com/$REPO_SLUG/blob/main/CHANGELOG.md\"|g" update.json
fi

# 3. Update update.json
sed -i "s/\"version\": \".*\"/\"version\": \"$TARGET_VER\"/" update.json
sed -i "s/\"versionCode\": [0-9]*/\"versionCode\": $TARGET_CODE/" update.json

if [ -n "$REPO_SLUG" ]; then
    NEW_URL="https://github.com/$REPO_SLUG/releases/download/$TARGET_VER/hid-gadget-module-$TARGET_VER.zip"
    sed -i "s|\"zipUrl\": \".*\"|\"zipUrl\": \"$NEW_URL\"|" update.json
else
    sed -i "s/hid-gadget-module.*.zip/hid-gadget-module-$TARGET_VER.zip/" update.json
fi

# 4. Update README badge
sed -i "s/version-v[0-9]\+\.[0-9]\+\.[0-9]\+/version-$TARGET_VER/" README.md

# 5. Update C Sources
# Pattern matching might need to be flexible
sed -i "s/HID INDUSTRIAL v[0-9]\+\.[0-9]\+\.[0-9]\+/HID INDUSTRIAL $TARGET_VER/" src/tui.c
sed -i "s/HID GADGET CONTROLLER v[0-9]\+\.[0-9]\+\.[0-9]\+/HID GADGET CONTROLLER $TARGET_VER/" src/hid-gadget.c

# 5. Build the static binaries for all architectures
echo "Building static binaries for all architectures..."
make static

# 6. Build WebUI server binary
echo "Building WebUI server..."
make hid-webui

# 7. Package the module
ZIP_NAME="hid-gadget-module-${TARGET_VER}.zip"
echo "Creating release ZIP: $ZIP_NAME"

zip -r "$ZIP_NAME" \
    blobs/ \
    system/ \
    META-INF/ \
    module.prop \
    service.sh \
    install.sh \
    uninstall.sh \
    customize.sh \
    webui/ \
    -x "*.git*" \
    -x "*.vscode*" \
    -x "src/*" \
    -x "include/*" \
    -x "tests/*" \
    -x "scripts/*" \
    -x "*.c" \
    -x "*.h" \
    -x "*.o" \
    -x "Makefile" \
    -x "hid-gadget" \
    -x "hid-gadget-mock"

# Copy WebUI binary to system/bin
echo "Adding WebUI binary to package..."
cp hid-webui system/bin/hid-webui-bin
zip -u "$ZIP_NAME" system/bin/hid-webui-bin
rm system/bin/hid-webui-bin

echo ""
echo "✅ Release built successfully!"
echo "   Version: $TARGET_VER"
echo "   File: $ZIP_NAME"
echo ""
