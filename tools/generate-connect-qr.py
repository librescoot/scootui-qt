#!/usr/bin/env python3
"""Regenerate the fixed Connect URL QR asset (development dependency: qrcode)."""
from pathlib import Path
import qrcode

URL = "https://connect.librescoot.org/pair"
qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=4)
qr.add_data(URL)
qr.make(fit=True)
matrix = qr.get_matrix()
size = len(matrix)
path = "".join(f"M{x},{y}h1v1h-1z" for y, row in enumerate(matrix) for x, ink in enumerate(row) if ink)
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size*5}" height="{size*5}">\n'
       f'<title>{URL}</title>\n<rect width="{size}" height="{size}" fill="white"/>\n'
       f'<path d="{path}" fill="black"/>\n</svg>\n')
Path(__file__).resolve().parents[1].joinpath("assets/icons/connect-pairing-qr.svg").write_text(svg)
