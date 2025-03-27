#!/bin/bash

set -eu

res=0

check_cmake-format() {
  while read -r file ; do
    if ! [ -f "${file}" ] ; then
      continue
    fi
    if [[ "${file}" =~ CMakeLists.txt$ ]] || [[ "${file}" =~ \.cmake$ ]] ; then
      echo "Checking ${file}"
      cmake-format --check -c cmake/cmake-format.py "${file}" || res=1
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
src_sha=$(git rev-parse -q --verify "${ref}") || die "fatal: couldn't find ref ${ref}"
echo "Checking $(git rev-list --count "${src_sha}" "^${target_sha}") commits since revision ${target_sha}"
for commit in $(git rev-list --reverse "${src_sha}" "^${target_sha}"); do
  check_cmake-format "${commit}"
done

exit ${res}
