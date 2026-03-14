#!/usr/bin/env bash
# flush.sh — flush the device sink buffer to HID output and clear it.
# Equivalent to pressing ENTER in the SinkProx cardputer app.
# Usage:
#   ./flush.sh
#
# Environment variables:
#   KPROX_API_ENDPOINT  — base URL, e.g. http://kprox.local
#   KPROX_API_KEY       — raw API key
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

echo "Flushing sink to HID output at ${KPROX_API_ENDPOINT}/api/flush..." >&2
response=$(_kprox_post /api/flush "{}")
echo "Response: $response" >&2
echo "Done." >&2
