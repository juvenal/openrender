#!/usr/bin/env bash
set -euo pipefail

SITE_DIR="${1:-docs/site/public}"

if [ ! -d "$SITE_DIR" ]; then
  echo "link-validator: site directory '$SITE_DIR' does not exist" >&2
  exit 1
fi

SITE_DIR="$(cd "$SITE_DIR" && pwd)"
broken=0

extract_links() {
  local file="$1"
  grep -ioE '(href|src)="[^"]*"' "$file" | sed -E 's/^[a-zA-Z]+="//; s/"$//'
  grep -ioE "(href|src)='[^']*'" "$file" | sed -E "s/^[a-zA-Z]+='//; s/'\$//"
}

resolve_target() {
  local src="$1" target="$2" dir path

  target="${target%%#*}"
  target="${target%%\?*}"

  if [ -z "$target" ]; then
    return 0
  fi

  if [[ "$target" == /* ]]; then
    path="$SITE_DIR$target"
  else
    dir="$(dirname "$src")"
    path="$dir/$target"
  fi

  if [[ "$path" == */ ]]; then
    path="${path}index.html"
  fi

  if [ -f "$path" ]; then
    return 0
  fi

  if [ -d "$path" ] && [ -f "$path/index.html" ]; then
    return 0
  fi

  echo "${src#"$SITE_DIR"/}: $2"
  broken=1
}

while IFS= read -r -d '' file; do
  while IFS= read -r target; do
    case "$target" in
      ''|http://*|https://*|//*|mailto:*|tel:*|javascript:*|data:*|'#'*) continue ;;
    esac
    resolve_target "$file" "$target"
  done < <(extract_links "$file")
done < <(find "$SITE_DIR" -type f -name '*.html' -print0)

exit $broken
