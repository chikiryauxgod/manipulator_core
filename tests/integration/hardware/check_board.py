#!/usr/bin/env python3
"""Compatibility entry point for the real-board unittest suite.

Pass --port explicitly. --flash enables persistent writes and ST-Link reset;
--output optionally writes a summary. Without a selected port, exits 77 (SKIP).
"""
import sys

from test_black_pill import main

if __name__ == '__main__':
    sys.exit(main())
