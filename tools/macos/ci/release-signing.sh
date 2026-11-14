#!/bin/bash
# [CHOSEN, not recovered] Release-only Developer ID signing and notarization for the macOS CI jobs
# (.github/workflows/build.yml, docs/ci-releases.md). Ad-hoc CI runs never call this script.
#
#   release-signing.sh import             MACOS_CERT_P12 (base64 .p12), MACOS_CERT_PASSWORD
#                                          -> temporary keychain; BF2_SIGN_IDENTITY to GITHUB_ENV
#   release-signing.sh notary-key         NOTARY_KEY_P8 (base64 .p8), NOTARY_KEY_ID
#                                          -> key file; BF2_NOTARY_KEY to GITHUB_ENV
#   release-signing.sh sign-standalone DIR  sign bin_macos (dylibs, bf2, bf2_client) for the ZIPs
#   release-signing.sh sign-dmg DMG       sign the disk image itself
#   release-signing.sh verify-dmg DMG     check the app inside: Developer ID, runtime, timestamp,
#                                          client entitlements
#   release-signing.sh notarize FILE      notarytool submit --wait; fails unless Accepted
#   release-signing.sh staple DMG         staple, validate, Gatekeeper assessment
#   release-signing.sh cleanup            delete the keychain and the key files (always run)
#
# Secret values are read from the environment only and are never echoed.
set -euo pipefail
set +x

here=$(cd "$(dirname "$0")" && pwd -P)
repo=$(cd "$here/../../.." && pwd -P)
client_entitlements="$repo/Code/Build/macos/bf2_client.entitlements"
work="${RUNNER_TEMP:?RUNNER_TEMP is not set}/openbf2-signing"
keychain="$work/openbf2-signing.keychain-db"

fail() { echo "::error::release-signing: $*" >&2; exit 1; }
need() { [[ -n "${!1:-}" ]] || fail "$1 is empty: add it to the 'release' environment secrets"; }
identity() { [[ -n "${BF2_SIGN_IDENTITY:-}" ]] || fail 'BF2_SIGN_IDENTITY is not set (run import first)'; }

# Every Mach-O gets the hardened runtime and a secure timestamp (both required for notarization).
sign() {
  /usr/bin/codesign --force --sign "$BF2_SIGN_IDENTITY" --options runtime --timestamp "$@"
}

case "${1:-}" in
import)
  need MACOS_CERT_P12
  need MACOS_CERT_PASSWORD
  umask 077
  mkdir -p "$work"
  password=$(/usr/bin/openssl rand -hex 24)
  /usr/bin/security create-keychain -p "$password" "$keychain"
  /usr/bin/security set-keychain-settings -lut 21600 "$keychain"
  /usr/bin/security unlock-keychain -p "$password" "$keychain"
  printf '%s' "$MACOS_CERT_P12" | /usr/bin/base64 --decode > "$work/cert.p12"
  /usr/bin/security import "$work/cert.p12" -k "$keychain" -f pkcs12 -P "$MACOS_CERT_PASSWORD" \
    -T /usr/bin/codesign -T /usr/bin/security > /dev/null
  rm -f "$work/cert.p12"
  /usr/bin/security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$password" "$keychain" > /dev/null
  # Search list: the temporary keychain first, then the existing user keychains.
  existing=()
  while IFS= read -r line; do
    line=${line#"${line%%[![:space:]]*}"}
    line=${line#\"}; line=${line%\"}
    [[ -n "$line" ]] && existing+=("$line")
  done < <(/usr/bin/security list-keychains -d user)
  /usr/bin/security list-keychains -d user -s "$keychain" "${existing[@]}"
  # Exactly one Developer ID Application identity; codesign gets its SHA-1 so names never clash.
  matches=$(/usr/bin/security find-identity -v -p codesigning "$keychain" | /usr/bin/grep '"Developer ID Application: ' || true)
  [[ $(printf '%s\n' "$matches" | /usr/bin/grep -c . || true) == 1 ]] ||
    fail 'MACOS_CERT_P12 must contain exactly one valid "Developer ID Application" identity'
  hash=$(printf '%s\n' "$matches" | /usr/bin/awk '{print $2}')
  name=$(printf '%s\n' "$matches" | /usr/bin/sed -E 's/^[^"]*"(.*)"$/\1/')
  echo "Signing identity: $name ($hash)"
  echo "BF2_SIGN_IDENTITY=$hash" >> "${GITHUB_ENV:?}"
  ;;
notary-key)
  need NOTARY_KEY_P8
  need NOTARY_KEY_ID
  [[ "$NOTARY_KEY_ID" =~ ^[A-Za-z0-9]+$ ]] || fail 'NOTARY_KEY_ID must be the alphanumeric key ID'
  umask 077
  mkdir -p "$work"
  key="$work/AuthKey_$NOTARY_KEY_ID.p8"
  printf '%s' "$NOTARY_KEY_P8" | /usr/bin/base64 --decode > "$key"
  /usr/bin/grep -q 'BEGIN PRIVATE KEY' "$key" || fail 'NOTARY_KEY_P8 does not decode to a .p8 private key'
  echo "BF2_NOTARY_KEY=$key" >> "${GITHUB_ENV:?}"
  ;;
sign-standalone)
  identity
  dir=${2:?usage: sign-standalone BIN_MACOS_DIR}
  [[ -f "$client_entitlements" ]] || fail "missing $client_entitlements"
  # Inside out: the libraries first, then the executables that load them.
  for item in libdice_py.dylib libaidll.dylib bf2; do
    sign "$dir/$item"
  done
  sign --entitlements "$client_entitlements" "$dir/bf2_client"
  for item in libdice_py.dylib libaidll.dylib bf2 bf2_client; do
    /usr/bin/codesign --verify --strict --verbose=2 "$dir/$item"
  done
  /usr/bin/codesign -d --entitlements - "$dir/bf2_client"
  ;;
sign-dmg)
  identity
  dmg=${2:?usage: sign-dmg DMG}
  /usr/bin/codesign --force --sign "$BF2_SIGN_IDENTITY" --timestamp "$dmg"
  /usr/bin/codesign --verify --strict --verbose=2 "$dmg"
  ;;
verify-dmg)
  dmg=${2:?usage: verify-dmg DMG}
  mkdir -p "$work"
  mount=$(mktemp -d "$work/mount.XXXXXX")
  /usr/bin/hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$mount" "$dmg" > /dev/null
  trap '/usr/bin/hdiutil detach -quiet "$mount" || /usr/bin/hdiutil detach -force -quiet "$mount"' EXIT
  app="$mount/OpenBF2.app"
  [[ -d "$app" ]] || fail 'no OpenBF2.app in the disk image'
  /usr/bin/codesign --verify --deep --strict --verbose=2 "$app"
  team=
  for item in "$app" "$app/Contents/MacOS/bf2_client" "$app/Contents/MacOS/libdice_py.dylib" "$app/Contents/MacOS/libaidll.dylib"; do
    info=$(/usr/bin/codesign -dvvv "$item" 2>&1)
    printf '%s\n' "$info" | /usr/bin/grep -q '^Authority=Developer ID Application: ' || fail "not Developer ID signed: $item"
    printf '%s\n' "$info" | /usr/bin/grep -Eq '^CodeDirectory .*flags=.*runtime' || fail "no hardened runtime: $item"
    printf '%s\n' "$info" | /usr/bin/grep -q '^Timestamp=' || fail "no secure timestamp: $item"
    this=$(printf '%s\n' "$info" | /usr/bin/sed -n 's/^TeamIdentifier=//p')
    [[ -z "$team" || "$team" == "$this" ]] || fail "Team ID differs ($this, expected $team): $item"
    team=$this
  done
  # The client needs the microphone entitlement for voice chat; the app's meter needs it too.
  for item in "$app" "$app/Contents/MacOS/bf2_client"; do
    /usr/bin/codesign -d --entitlements - --xml "$item" 2>/dev/null |
      /usr/bin/grep -q 'com.apple.security.device.audio-input' || fail "audio-input entitlement missing: $item"
  done
  echo "App and client signed by Team ID $team with hardened runtime and timestamp"
  ;;
notarize)
  file=${2:?usage: notarize FILE}
  need NOTARY_KEY_ID
  need NOTARY_ISSUER_ID
  [[ -f "${BF2_NOTARY_KEY:-}" ]] || fail 'BF2_NOTARY_KEY is not set (run notary-key first)'
  auth=(--key "$BF2_NOTARY_KEY" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_ISSUER_ID")
  result="$work/notary-$(basename "$file").json"
  status=0
  /usr/bin/xcrun notarytool submit "$file" "${auth[@]}" --wait --timeout 2h --output-format json > "$result" || status=$?
  cat "$result"; echo
  verdict=$(/usr/bin/plutil -extract status raw -o - "$result" 2>/dev/null || true)
  id=$(/usr/bin/plutil -extract id raw -o - "$result" 2>/dev/null || true)
  if [[ "$verdict" != Accepted ]]; then
    [[ -n "$id" ]] && /usr/bin/xcrun notarytool log "$id" "${auth[@]}" || true
    fail "notarization of $(basename "$file") ended with '${verdict:-no status}' (notarytool exit $status)"
  fi
  echo "Notarized: $(basename "$file") ($id)"
  ;;
staple)
  dmg=${2:?usage: staple DMG}
  /usr/bin/xcrun stapler staple "$dmg"
  /usr/bin/xcrun stapler validate "$dmg"
  /usr/sbin/spctl -a -t open --context context:primary-signature -v "$dmg"
  ;;
cleanup)
  if [[ -f "$keychain" ]]; then
    /usr/bin/security delete-keychain "$keychain" || true
  fi
  rm -rf "$work"
  ;;
*)
  echo 'usage: release-signing.sh import|notary-key|sign-standalone DIR|sign-dmg DMG|verify-dmg DMG|notarize FILE|staple DMG|cleanup' >&2
  exit 2
  ;;
esac
