#!/bin/sh
set -e

# This script builds and installs libvalkey using GNU Make and libvalkeycluster using CMake.
# The shared library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")
repo_dir=$(git rev-parse --show-toplevel)

# Build and install libvalkey from the repo using GNU Make
make -C ${repo_dir}/libvalkey \
     USE_SSL=1 \
     DESTDIR=${script_dir}/install \
     clean all install


# Build and install libvalkey-cluster from the repo using CMake.
mkdir -p ${script_dir}/libvalkeycluster_build
cd ${script_dir}/libvalkeycluster_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON \
      -DCMAKE_PREFIX_PATH=${script_dir}/install/usr/local \
      ${repo_dir}/libvalkeycluster
make DESTDIR=${script_dir}/install install


# Build example binaries by providing shared libraries
make -C ${repo_dir}/libvalkeycluster \
     CFLAGS="-I${script_dir}/install/usr/local/include" \
     LDFLAGS="-lvalkeycluster -lvalkeycluster_ssl -lvalkey -lvalkey_ssl \
              -L${script_dir}/install/usr/local/lib/ \
              -Wl,-rpath=${script_dir}/install/usr/local/lib/" \
     USE_SSL=1 \
     clean examples
