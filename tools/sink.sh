#!/usr/bin/env bash
# sink.sh — pipe stdin (or a string argument) into the device sink buffer.
# Data accumulates in /sink.txt on the device until flushed to HID output.
# Usage:
#   echo "hello world" | ./sink.sh
#   ./sink.sh "hello world"
#   cat file.txt | ./sink.sh
#
# Environment variables (same as kpipe.sh):
#   KPROX_API_ENDPOINT  — base URL, e.g. http://kprox.local
#   KPROX_API_KEY       — raw API key
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

# Accept data from argument or stdin
if [[ $# -gt 0 ]]; then
    full_text="$*"
else
    full_text=""
    while IFS= read -r line; do
        full_text+="$line"$'\n'
    done
    full_text="${full_text%$'\n'}"
fi

if [[ -z "$full_text" ]]; then
    echo "Error: no input provided." >&2
    exit 1
fi

# Escape for JSON
json_text=$(printf '%s' "$full_text" | python3 -c "
import sys, json
text = sys.stdin.read()
print(json.dumps(text), end='')
")

echo "Sending ${#full_text} characters to sink at ${KPROX_API_ENDPOINT}/api/sink..." >&2
_kprox_post /api/sink "{\"text\":${json_text}}" > /dev/null
echo "Done. Use flush.sh to send sink contents to HID output." >&2
