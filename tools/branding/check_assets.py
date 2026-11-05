# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
from pathlib import Path
import sys

from assets import validate

try:
    validate(Path(__file__).resolve().parents[2])
except (OSError, ValueError) as error:
    print(f"Boot branding refused: {error}", file=sys.stderr)
    raise SystemExit(1)
print("Boot branding matches the existing QorMark sources and reviewed image")
