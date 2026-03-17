#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
parent_dir=$(dirname -- "$repo_root")
artifact_dir="$repo_root/dist/debian"

mkdir -p "$artifact_dir"
rm -f "$artifact_dir"/*.deb "$artifact_dir"/*.ddeb "$artifact_dir"/*.buildinfo "$artifact_dir"/*.changes

cd "$repo_root"
dpkg-buildpackage -us -uc -b

for pattern in *.deb *.ddeb *.buildinfo *.changes; do
    found=false
    for file in "$parent_dir"/$pattern; do
        if [ -e "$file" ]; then
            mv "$file" "$artifact_dir"/
            found=true
        fi
    done
    if [ "$found" = false ]; then
        :
    fi
done

printf 'Debian artifacts collected in %s\n' "$artifact_dir"