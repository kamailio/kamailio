#!/bin/bash
COMMIT_MESSAGE_SUBJECT_FORMAT="%s"

set -eu

res=0

die() { echo "$@" >&2; exit 1; }
fail() { echo "*** $@ ***" >&2; res=1; }

git_log_format() {
  local pattern="$1"
  local reference="$2"
  git log -1 --pretty=format:"$pattern" "$reference"
}

check_subject() {
  local commit="$1"
  local subject="$2"

  # get the prefix
  if ! [[ "${subject}" =~ ^([^:]+):.*$ ]] ; then
    fail "[${commit}] prefix not detected:'${subject}'"
    return
  fi
  prefix="${BASH_REMATCH[1]}"
  if [ -z "${prefix}" ] ; then
    fail "[${commit}] commit subject has no prefix:'${subject}'"
    return
  fi

  # core or lib
  if [ -d "src/${prefix}" ] ; then
    echo "[${commit}] prefix is core or lib, OK[${prefix}]"
    return
  fi

  # prefix is a module
  if [ -d "src/modules/${prefix}" ] ; then
    echo "[${commit}] prefix is module, OK[${prefix}]"
    return
  fi

  # utils?
  if [ -d "${prefix}" ] ; then
    echo "[${commit}] prefix is a dir in the repo, OK[${prefix}]"
    return
  fi

  # github configs
  if [[ "${prefix}" =~ ^github$ ]] ; then
    echo "[${commit}] prefix is github config, OK[${prefix}]"
    return
  fi

  fail "[${commit}] unknown prefix:'${prefix}'"
}

target="${GITHUB_BASE_REF:-master}"
if [ "${CI:-}" ]; then
    git rev-parse -q --no-revs --verify "origin/${target}" || \
        git rev-parse -q --no-revs --verify "${target}" || \
        git fetch origin --depth=1 "${target}"
    git rev-parse -q --no-revs --verify "origin/${target}" || \
        git rev-parse -q --no-revs --verify "${target}" || \
        git fetch origin --depth=1 tag "${target}"
    # Ensure that the target revision has some history
    target_sha=$(git rev-parse -q --verify "origin/${target}" || git rev-parse -q --verify "${target}")
    git fetch -q "--depth=${FETCH_DEPTH:-50}" origin "+${target_sha}"
else
    target_sha=$(git rev-parse -q --verify "${target}") || die "fatal: couldn't find ref ${target}"
fi

ref=${ref:-HEAD}
src_sha=$(git rev-parse -q --verify "${ref}") || die "fatal: couldn't find ref ${ref}"
echo "Checking $(git rev-list --count "${src_sha}" "^${target_sha}") commits since revision ${target_sha}"
for commit in $(git rev-list --reverse "${src_sha}" "^${target_sha}"); do
  COMMIT_MESSAGE_SUBJECT=$(git_log_format "${COMMIT_MESSAGE_SUBJECT_FORMAT}" "${commit}")
  check_subject "${commit}" "${COMMIT_MESSAGE_SUBJECT}"
done

echo "Result: ${res}"
exit $res
