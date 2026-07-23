#!/usr/bin/env bash
exec "$(dirname "$0")/../.venv/bin/python" "$(dirname "$0")/../service/main.py"
