#!/usr/bin/env bash

set -euo pipefail

dry_run=0

usage() {
	echo "Usage: $0 [--dry-run]"
}

if [[ $# -gt 1 ]]; then
	usage
	exit 1
fi

if [[ $# -eq 1 ]]; then
	case "$1" in
		--dry-run)
			dry_run=1
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			usage
			exit 1
			;;
	esac
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

cd "${repo_root}"

move_makefiles_tree() {
	local tree_root="$1"
	local dst_root="${repo_root}/misc/makefiles/${tree_root}"
	local src_root="${repo_root}/${tree_root}"

	find "${src_root}" -type f \( -name 'Makefile' -o -name 'Makefile.*' \) | while read -r src_file; do
		local rel_path="${src_file#${src_root}/}"
		local dst_file="${dst_root}/${rel_path}"

		if [[ ${dry_run} -eq 1 ]]; then
			printf 'git mv %s %s\n' "${src_file}" "${dst_file}"
		else
			mkdir -p "$(dirname "${dst_file}")"
			git mv "${src_file}" "${dst_file}"
		fi
	done
}

move_makefiles_tree "src"
move_makefiles_tree "doc"
move_makefiles_tree "utils"
