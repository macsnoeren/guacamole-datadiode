#!/bin/sh
# Run the Meson tests for a build directory, honouring the DISABLE_TESTS build
# arg (exported into the RUN environment). Used by the broker Dockerfiles so the
# skip logic lives in one place instead of being duplicated per project.
#
# Usage: run-tests.sh <build-dir> [<build-dir> ...]
set -e

for build_dir in "$@"; do
    if [ "$DISABLE_TESTS" = "true" ]; then
        echo "Warning: Skipping tests for $build_dir!"
    else
        meson test -C "$build_dir" || (cat "$build_dir/meson-logs/testlog.txt" ; exit 1)
    fi
done
