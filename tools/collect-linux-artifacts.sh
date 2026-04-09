#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_root/build"
artifact_dir="$repo_root/dist/linux"
staging_dir="$artifact_dir/bluraybackup-ex.linux_x64"
tar_path="$artifact_dir/bluraybackup-ex.linux_x64.tar.gz"
binary_name="bluraybackup-ex"

mkdir -p "$artifact_dir"
rm -rf "$artifact_dir"/*

candidate_dirs="$build_dir $build_dir/Release $build_dir/RelWithDebInfo $build_dir/MinSizeRel $build_dir/Debug"

source_dir=""
for dir in $candidate_dirs; do
    if [ -f "$dir/$binary_name" ]; then
        source_dir="$dir"
        break
    fi
done

if [ -z "$source_dir" ]; then
    printf 'ERROR: Could not find %s in any expected build output directory under %s\n' "$binary_name" "$build_dir" >&2
    exit 1
fi

mkdir -p "$staging_dir"
cp "$source_dir/$binary_name" "$staging_dir/$binary_name"

tar -czf "$tar_path" -C "$staging_dir" .

printf 'Linux artifact collected: %s\n' "$tar_path"
