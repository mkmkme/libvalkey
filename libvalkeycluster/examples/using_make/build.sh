#!/bin/sh
set -e

# This script builds and installs libvalkey and libvalkeycluster using GNU Make directly.
# The static library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")
repo_dir=$(git rev-parse --show-toplevel)

# Build and install libvalkey from the repo using GNU Make
make -C ${repo_dir}/libvalkey \
     USE_SSL=1 \
     DESTDIR=${script_dir}/install \
     clean all install


# Build and install libvalkeycluster from the repo using GNU Make
make -C ${repo_dir}/libvalkeycluster \
     CFLAGS="-I${script_dir}/install/usr/local/include" \
     LDFLAGS="-L${script_dir}/install/usr/local/lib" \
     USE_SSL=1 \
     DESTDIR=${script_dir}/install \
     clean all install


# Build example binaries by providing static libraries
make -C ${repo_dir}/libvalkeycluster \
     CFLAGS="-I${script_dir}/install/usr/local/include" \
     LDFLAGS="${script_dir}/install/usr/local/lib/libvalkeycluster.a \
              ${script_dir}/install/usr/local/lib/libvalkeycluster_ssl.a \
              ${script_dir}/install/usr/local/lib/libvalkey.a \
              ${script_dir}/install/usr/local/lib/libvalkey_ssl.a" \
     USE_SSL=1 \
     clean examples
