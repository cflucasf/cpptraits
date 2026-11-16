#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

examples=(basic external_mapping ownership defaults static_traits composition
          concrete_methods class_template associated_types)

case "${1:-}" in
    --list)
        printf '%s\n' "${examples[@]}"
        exit 0
        ;;
    -h|--help)
        printf 'Usage: ./run.sh [example] [args...]\n       ./run.sh --list\n\nDefaults to basic. Run ./build.sh to build the examples.\n'
        exit 0
        ;;
esac

example="${1:-basic}"
if (( $# > 0 )); then
    shift
fi

case "$example" in
    basic|external_mapping|ownership|defaults|static_traits|composition|concrete_methods|class_template|associated_types) ;;
    *)
        printf 'Unknown example: %s\nUse ./run.sh --list to see available examples.\n' "$example" >&2
        exit 2
        ;;
esac

executable="./build/debug/examples/$example"
if [[ ! -x "$executable" ]]; then
    printf 'Example executable not found: %s\nRun ./build.sh first. If examples were disabled, run:\n  cmake --preset debug -DCPPTRAITS_BUILD_EXAMPLES=ON && ./build.sh\n' "$executable" >&2
    exit 1
fi

exec "$executable" "$@"
