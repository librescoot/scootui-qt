#!/usr/bin/env python3
"""Decode XCB captures from CloudPairingScreenTest (requires pillow and zxing-cpp)."""
import sys
from pathlib import Path

from PIL import Image
import zxingcpp

captures = Path(sys.argv[1])
for language in ("en", "de"):
    for theme in ("dark", "light"):
        for state in ("waiting_for_web_setup", "starting", "stopping", "connected", "error", "paused"):
            image = Image.open(captures / f"{language}-{theme}-{state}.png")
            result = zxingcpp.read_barcode(image)
            expected = "https://connect.librescoot.org/pair#code=WWWWW-WWWWW" if state == "waiting_for_web_setup" else None
            assert (result.text if result else None) == expected, (language, theme, state)
for name, code in (("en-dark-code2", "01234-ABCDE"), ("de-light-code3", "6789A-BCDEF")):
    result = zxingcpp.read_barcode(Image.open(captures / f"{name}.png"))
    assert result and result.text == "https://connect.librescoot.org/pair#code=" + code, name
print("QR captures decoded and absent states checked")
