#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "${project_root}"

build_dir="${project_root}/build"
build_type="Debug"

case "${1:-debug}" in
  debug)   build_type="Debug" ;;
  release) build_type="Release" ;;
  profile) build_type="RelWithDebInfo" ;;
  clean|rebuild)
    if [[ -L "${build_dir}" ]]; then
      echo "ERROR: build/ is a symbolic link; refusing to remove it." >&2
      exit 1
    fi
    rm -rf -- "${build_dir}"
    if [[ "${1}" == "clean" ]]; then
      echo "Removed build/"
      exit 0
    fi
    ;;
  test)
    if [[ ! -f "${build_dir}/CTestTestfile.cmake" ]]; then
      echo "ERROR: no configured tests. Build the project first using ./m." >&2
      exit 1
    fi
    cmake --build "${build_dir}"
    ctest --test-dir "${build_dir}" --output-on-failure
    exit 0
    ;;
  *)
    echo "Usage: ./m [debug|release|profile|clean|rebuild|test]" >&2
    exit 2
    ;;
esac

if [[ ! -f CMakeLists.txt ]]; then
  echo "ERROR: CMakeLists.txt not found. ZXFont core has not been bootstrapped yet." >&2
  exit 1
fi

for tool in cmake ninja; do
  if ! command -v "${tool}" >/dev/null 2>&1; then
    echo "ERROR: ${tool} is required for building ZXFont." >&2
    exit 1
  fi
done

cmake -S . -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE="${build_type}"
cmake --build "${build_dir}"
