from __future__ import annotations

import sys

from vdtrace_gui.console_app import run_cli
from vdtrace_gui.models import repo_root_from_file


def main() -> int:
    return run_cli(repo_root_from_file(__file__))


if __name__ == "__main__":
    sys.exit(main())
