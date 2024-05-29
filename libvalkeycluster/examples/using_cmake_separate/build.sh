#!/bin/sh
set -e

# This script builds and installs libvalkey and libvalkeycluster as separate
# steps using CMake.
# The shared library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")
repo_dir=$(git rev-parse --show-toplevel)

# Build and install libvalkey from the repo using CMake.
mkdir -p ${script_dir}/libvalkey_build
cd ${script_dir}/libvalkey_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON \
      ${repo_dir}/libvalkey
make DESTDIR=${script_dir}/install install


# Build and install libvalkeycluster from the repo using CMake.
mkdir -p ${script_dir}/libvalkeycluster_build
cd ${script_dir}/libvalkeycluster_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON \
      -DCMAKE_PREFIX_PATH=${script_dir}/install/usr/local \
      ${repo_dir}/libvalkeycluster
make DESTDIR=${script_dir}/install install


# Build examples using headers and libraries installed in previous steps.
mkdir -p ${script_dir}/example_build
cd ${script_dir}/example_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_SSL=ON \
      -DCMAKE_PREFIX_PATH=${script_dir}/install/usr/local \
      ${script_dir}/../src
make
