#!/usr/bin/env bash
set -euo pipefail

TAG="${1:?tag is required}"
VERSION="${2:?version is required}"
COMMIT="${3:?commit is required}"
REPOSITORY="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

if [[ "$TAG" != "v$VERSION" ]]; then
  echo "Tag $TAG does not match VERSION $VERSION."
  exit 1
fi

if [[ ! "$COMMIT" =~ ^[0-9a-fA-F]{40}$ ]]; then
  echo "Commit must be a 40-character Git SHA."
  exit 1
fi

for asset in Asterun-x64.zip Asterun-ARM64.zip SHA256SUMS.txt update-manifest.json; do
  test -f "$asset"
done

test "$(wc -l < SHA256SUMS.txt)" -eq 2
sha256sum -c SHA256SUMS.txt

IS_PRERELEASE=false
if [[ "$VERSION" == *-* ]]; then
  IS_PRERELEASE=true
fi

jq -e --arg version "$VERSION" --arg commit "${COMMIT,,}" --argjson prerelease "$IS_PRERELEASE" '.schemaVersion == 1 and .version == $version and .commit == $commit and .prerelease == $prerelease and .assets.x64.name == "Asterun-x64.zip" and .assets.ARM64.name == "Asterun-ARM64.zip"' update-manifest.json >/dev/null

REMOTE_SHA="$(git ls-remote --tags origin "refs/tags/$TAG" | awk '{print $1}')"

if [[ -z "$REMOTE_SHA" ]]; then
  git tag "$TAG" "$COMMIT"
  git push origin "refs/tags/$TAG"
elif [[ "$REMOTE_SHA" != "$COMMIT" ]]; then
  echo "Versioned tag $TAG is immutable: remote=$REMOTE_SHA candidate=$COMMIT"
  exit 1
fi

verify_release() {
  local release_json="$1"

  jq -e --arg tag "$TAG" --argjson prerelease "$IS_PRERELEASE" '.tag_name == $tag and .draft == false and .prerelease == $prerelease and ([.assets[].name] | sort) == (["Asterun-ARM64.zip", "Asterun-x64.zip", "SHA256SUMS.txt", "update-manifest.json"] | sort)' "$release_json" >/dev/null

  local public_manifest_url="https://github.com/$REPOSITORY/releases/download/$TAG/update-manifest.json"
  local verified=false
  rm -f published-update-manifest.json

  for attempt in $(seq 1 20); do
    if curl --location --fail --silent --show-error "$public_manifest_url" -o published-update-manifest.json && cmp -s update-manifest.json published-update-manifest.json; then
      verified=true
      break
    fi

    rm -f published-update-manifest.json
    sleep 3
  done

  if [[ "$verified" != "true" ]]; then
    echo "Published $TAG manifest did not become anonymously downloadable and byte-identical."
    exit 1
  fi
}

existing_release_id="$(gh api --paginate --slurp "repos/$REPOSITORY/releases?per_page=100" | jq -r --arg tag "$TAG" '[.[][] | select(.tag_name == $tag) | .id][0] // empty')"
release_json="existing-release.json"

if [[ -n "$existing_release_id" ]]; then
  gh api "repos/$REPOSITORY/releases/$existing_release_id" > "$release_json"

  if [[ "$(jq -r '.draft' "$release_json")" == "true" ]]; then
    echo "Recovering interrupted draft release $TAG (id $existing_release_id)."
    gh api --method DELETE "repos/$REPOSITORY/releases/$existing_release_id" >/dev/null
  else
    echo "$TAG is already published; verifying immutable release assets."
    verify_release "$release_json"
    exit 0
  fi
fi

extra_args=()
if [[ "$IS_PRERELEASE" == "true" ]]; then
  extra_args+=(--prerelease)
fi

gh release create "$TAG" Asterun-x64.zip Asterun-ARM64.zip SHA256SUMS.txt update-manifest.json --draft "${extra_args[@]}" --title "Asterun $TAG" --notes "Versioned build created automatically from commit $COMMIT."

release_id="$(gh api --paginate --slurp "repos/$REPOSITORY/releases?per_page=100" | jq -r --arg tag "$TAG" '[.[][] | select(.tag_name == $tag) | .id][0] // empty')"
test -n "$release_id"

gh api --method PATCH "repos/$REPOSITORY/releases/$release_id" -F draft=false -F prerelease="$IS_PRERELEASE" >/dev/null
gh api "repos/$REPOSITORY/releases/$release_id" > published-release.json

verify_release published-release.json

echo "$TAG release contract is public, immutable, and coherent."
