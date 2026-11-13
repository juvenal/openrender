#!/usr/bin/env bash
# Spec 056 (tasks.md T045): the golden capture tool; see capture_goldens.py.
exec python3 "$(dirname "$0")/capture_goldens.py" "$@"
