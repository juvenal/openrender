#!/usr/bin/env bash
# Create release/X.Y and vX.Y.0 from a master commit, then open a pull request
# that records the end-of-support date. Does not push to master.
set -euo pipefail

version="${VERSION:?}"
requested="${COMMIT:-}"

if ! [[ "${version}" =~ ^[0-9]+\.[0-9]+$ ]]; then
    echo "version must be X.Y (for example 1.0), got '${version}'" >&2
    exit 1
fi

git fetch origin master
if [ -z "${requested}" ]; then
    commit="$(git rev-parse origin/master)"
else
    commit="$(git rev-parse --verify "${requested}^{commit}")"
fi
if ! [[ "${commit}" =~ ^[0-9a-f]{40}$ ]]; then
    echo "could not resolve a commit sha" >&2
    exit 1
fi
if ! git merge-base --is-ancestor "${commit}" origin/master; then
    echo "${commit} is not contained in origin/master" >&2
    exit 1
fi

branch="release/${version}"
tag="v${version}.0"
record="lts-record/${version}"

if git ls-remote --exit-code --heads origin "${branch}" >/dev/null 2>&1; then
    echo "${branch} already exists" >&2
    exit 1
fi
if git ls-remote --exit-code --tags origin "${tag}" >/dev/null 2>&1; then
    echo "${tag} already exists" >&2
    exit 1
fi
if git ls-remote --exit-code --heads origin "${record}" >/dev/null 2>&1; then
    echo "${record} already exists; delete it before cutting this line again" >&2
    exit 1
fi

git push origin "${commit}:refs/heads/${branch}"
git -c user.name="github-actions[bot]" \
    -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
    tag -a "${tag}" "${commit}" -m "openRender ${version}.0"
git push origin "${tag}"

git checkout -B "${record}" origin/master
python3 scripts/ci/record_lts.py \
    --version "${version}" \
    --commit "${commit}" \
    --cut-date "$(date -u +%Y-%m-%d)"
git -c user.name="github-actions[bot]" \
    -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
    commit -am "Record release/${version} end of support"
git push origin "HEAD:${record}"

gh pr create \
    --base master \
    --head "${record}" \
    --title "Record LTS ${version} support window" \
    --body "$(cat <<EOF
Records \`release/${version}\` in \`LTS.json\` and \`.github/dependabot.yml\`.

- Branch: \`${branch}\` at \`${commit}\`
- First tag: \`${tag}\` (this push starts the LTS release workflow)
- End of support: two years after $(date -u +%Y-%m-%d)

Merging this pull request does not change the release branch. The branch and tag were created by \`cut-lts.yml\` and are not a push to master.
EOF
)"
