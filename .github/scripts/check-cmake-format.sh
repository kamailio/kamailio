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

check_cmake-format() {
  while read -r file ; do
    if ! [ -f "${file}" ] ; then
      continue
    fi
    if [[ "${file}" =~ CMakeLists.txt$ ]] || [[ "${file}" =~ \.cmake$ ]] ; then
      printf "Checking %s" "${file}"
      if cmake-format --check --config-files=cmake/cmake-format.py "${file}" ; then
        printf ". OK\n"
      else
        printf ". Fail\n"
        res=1
      fi
    fi
  done < <(git diff-tree --no-commit-id --name-only -r "${1}")
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
# Ensure that the ref revision has some history
git fetch -q "--depth=${FETCH_DEPTH:-50}" origin "+${ref}"
src_sha=$(git rev-parse -q --verify "${ref}") || die "fatal: couldn't find ref ${ref}"
echo "Checking $(git rev-list --count "${src_sha}" "^${target_sha}") commits since revision ${target_sha}"
for commit in $(git rev-list --reverse "${src_sha}" "^${target_sha}"); do
  commit_msg=$(git_log_format "${COMMIT_MESSAGE_SUBJECT_FORMAT}" "${commit}")
  echo "[${commit}] ${commit_msg}"
  git checkout --progress --force "${commit}"
  check_cmake-format "${commit}"
  echo "===================== done ========= "
done

echo "Result: ${res}"
exit ${res}
