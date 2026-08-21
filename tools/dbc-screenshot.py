#!/usr/bin/env python3
"""Dump the DBC's live display to a PPM by reading the DRM scanout buffer.

scootui-qt renders through eglfs_kms and sets its own mode, so /dev/fb0 (the
imx-drm fbdev emulation, still holding the fbcon buffer) is not what the CRTC
scans out. Find the active framebuffer through debugfs instead and read its
physical memory. Needs CONFIG_STRICT_DEVMEM unset, which is the case here.

Usage: dbc-screenshot.py [out.ppm]
"""
import mmap, os, re, sys

DRI = "/sys/kernel/debug/dri/1"   # imx-drm; card0 is etnaviv (GPU, no KMS)

def active_fb_id():
    state = open(f"{DRI}/state").read()
    # The primary plane is the one with a real CRTC attached.
    for block in state.split("plane[")[1:]:
        crtc = re.search(r"crtc=(\S+)", block)
        fb = re.search(r"fb=(\d+)", block)
        if crtc and fb and crtc.group(1) != "(null)" and fb.group(1) != "0":
            return int(fb.group(1))
    raise SystemExit("no active plane: is anything driving the display?")

def fb_info(fb_id):
    text = open(f"{DRI}/framebuffer").read()
    for block in text.split("framebuffer[")[1:]:
        if not block.startswith(f"{fb_id}]"):
            continue
        g = lambda p: re.search(p, block)
        size = g(r"size=(\d+)x(\d+)")
        pitch = g(r"pitch\[0\]=(\d+)")
        addr = g(r"dma_addr=0x([0-9a-fA-F]+)")
        fmt = g(r"format=(\S+)")
        if not (size and pitch and addr):
            raise SystemExit(f"framebuffer[{fb_id}] has no dma_addr (not CMA-backed?)")
        return (int(size.group(1)), int(size.group(2)), int(pitch.group(1)),
                int(addr.group(1), 16), fmt.group(1) if fmt else "?")
    raise SystemExit(f"framebuffer[{fb_id}] not listed")

def main():
    fb_id = active_fb_id()
    w, h, pitch, addr, fmt = fb_info(fb_id)
    sys.stderr.write(f"fb[{fb_id}] {w}x{h} {fmt} pitch={pitch} @ 0x{addr:08x}\n")

    fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
    try:
        m = mmap.mmap(fd, pitch * h, mmap.MAP_SHARED, mmap.PROT_READ, offset=addr)
    finally:
        os.close(fd)

    out = open(sys.argv[1], "wb") if len(sys.argv) > 1 else sys.stdout.buffer
    out.write(b"P6\n%d %d\n255\n" % (w, h))
    for y in range(h):
        row = m[y * pitch:y * pitch + w * 4]
        if fmt.startswith("XR24") or fmt.startswith("AR24"):
            # BGRX in memory, little-endian XRGB8888
            out.write(bytes(b for i in range(0, w * 4, 4)
                            for b in (row[i + 2], row[i + 1], row[i])))
        elif fmt.startswith("RG16"):
            row = m[y * pitch:y * pitch + w * 2]
            px = bytearray()
            for i in range(0, w * 2, 2):
                v = row[i] | (row[i + 1] << 8)
                px += bytes((((v >> 11) & 0x1F) << 3, ((v >> 5) & 0x3F) << 2, (v & 0x1F) << 3))
            out.write(bytes(px))
        else:
            raise SystemExit(f"unhandled format {fmt}")
    out.flush()
    m.close()

main()
