# Testing scootui-qt on a real DBC

Two things make on-target testing awkward: you cannot see what the UI is
drawing, and you cannot set up an interesting vehicle state without operating
the actual vehicle. Both have workarounds that need no unlock and touch no
production state.

## Screenshots: why /dev/fb0 is empty

Reading `/dev/fb0` gets you a black image no matter what is on screen. The node
exists and has the right geometry, which makes the result look plausible and
wrong.

The DBC has two DRM devices, and they are not interchangeable:

| Node | Driver | What it is |
|---|---|---|
| `card0`, `renderD128` | `etnaviv` | GC2000 GPU. Renders. No connectors, no KMS. |
| `card1` | `imx-drm` | IPU display controller. Owns the `DPI-1` connector and the CRTC. |
| `/dev/fb0` | `imx-drm` | fbdev emulation on the *same* device as `card1` |

`/dev/fb0` is not a separate display. It is a compatibility buffer on the
imx-drm device, allocated by `[fbcon]`, which is what the kernel logo and
Plymouth draw into early in boot. scootui-qt runs `eglfs_kms` against
`card1` (see `/etc/scootui-qt-kms.json`), allocates its own framebuffers, and
sets its own mode. From that point the CRTC scans out a Qt buffer and the fbcon
buffer is simply no longer displayed. It keeps whatever was in it, which after
Plymouth quits is black.

So the content lives in Qt's framebuffers, and those are CMA allocations you can
find and read.

`/sys/kernel/debug/dri/1/state` names the framebuffer currently on the primary
plane:

```
plane[32]: plane-0
	crtc=crtc-0
	fb=55
		format=XR24 little-endian
```

`/sys/kernel/debug/dri/1/framebuffer` then gives that id a physical address:

```
framebuffer[55]:
	allocated by = scootui-qt
	size=480x480
		pitch[0]=1920
			dma_addr=0x40600000
```

`CONFIG_STRICT_DEVMEM` is not set on this kernel, so that address is readable
through `/dev/mem`.

### The tool

`tools/dbc-screenshot.py` does the above and writes a PPM. Copy it over and run
it:

```bash
cat tools/dbc-screenshot.py | ssh -J deep-blue root@192.168.7.2 "cat > /data/dbc-screenshot.py"
ssh -J deep-blue root@192.168.7.2 "python3 /data/dbc-screenshot.py /data/shot.ppm"
ssh -J deep-blue root@192.168.7.2 "cat /data/shot.ppm" > shot.ppm
magick shot.ppm shot.png     # or: python3 -c "from PIL import Image; Image.open('shot.ppm').save('shot.png')"
```

PPM because the DBC has no image library, and the format is three bytes per
pixel with a text header. Nothing to install on the target.

Two things to know when reading buffers by hand:

Qt's framebuffers are `XR24` (XRGB8888, 32bpp, byte order BGRX), while the panel
itself is RGB565 and `/dev/fb0` reports 16bpp. Do not assume the panel's format.
The IPU converts on scanout.

Rendering is triple buffered, so the three Qt framebuffers hold different
frames. `state` tells you which one is live. If you sample all three instead,
take the median rather than the first: right after a repaint one buffer is
typically still showing the previous frame.

## Driving state without touching the vehicle

scootui-qt picks its Redis from `SCOOTUI_REDIS_HOST` (`host:port`, or `none` for
the built-in simulator). Point a second instance at a throwaway Redis and you
can set any vehicle state you like, with no risk of a stray `PUBLISH` on the
`vehicle` channel reaching battery-service, pm-service, alarm-service and the
rest.

Start a scratch instance on the MDB, on a port the vehicle does not use:

```bash
ssh deep-blue 'mkdir -p /tmp/testredis && redis-server --port 6380 \
  --bind 192.168.7.1 127.0.0.1 --protected-mode no --save "" --appendonly no \
  --dir /tmp/testredis --daemonize yes --logfile /tmp/testredis/redis.log'
```

Seed enough for the screen you want. A parked cluster needs little:

```bash
ssh deep-blue 'redis-cli -p 6380 hset vehicle state parked kickstand down seatbox closed blinker off
redis-cli -p 6380 hset settings dashboard.theme auto dashboard.backlight-mode auto dashboard.language en
redis-cli -p 6380 hset dashboard brightness 900 backlight 10240 backlight-enabled true ready true
redis-cli -p 6380 hset engine-ecu speed 42 rpm 1800 odometer 12345678 state on
redis-cli -p 6380 hset battery:0 present true charge 72 state active'
```

Then stop the real instance and run one by hand with the unit's environment plus
the override. The env matters: without it Qt picks the wrong platform plugin and
fails to take the display.

```bash
ssh -J deep-blue root@192.168.7.2 '
systemctl stop scootui-qt.service
export LANG=C.UTF-8 HOME=/data XDG_RUNTIME_DIR=/run/user/0 \
  MESA_SHADER_CACHE_DIR=/var/volatile/mesa_shader_cache \
  QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_INTEGRATION=eglfs_kms \
  QT_QPA_EGLFS_KMS_CONFIG=/etc/scootui-qt-kms.json QT_QPA_EGLFS_KMS_ATOMIC=1 \
  QT_QPA_EGLFS_SWAPINTERVAL=1 QSG_RENDER_LOOP=basic \
  QT_PLUGIN_PATH=/usr/plugins:/usr/lib/plugins \
  SCOOTUI_REDIS_HOST=192.168.7.1:6380
nohup /usr/bin/scootui-qt > /data/t.log 2>&1 &'
```

Startup to first frame is about 12 seconds. Afterwards:

```bash
ssh -J deep-blue root@192.168.7.2 'kill $(pidof scootui-qt); sleep 3; systemctl start scootui-qt.service'
ssh deep-blue 'redis-cli -p 6380 shutdown nosave; rm -rf /tmp/testredis'
```

Check `redis-cli hget vehicle state` on the real instance before and after. It
should not have moved.

## Gotchas

The screen is mostly black in `stand-by`. Anything you are trying to see needs
`vehicle[state]` to be `parked` or `ready-to-drive`, which is the main reason
the scratch Redis is worth the setup rather than reading the live one.

`dashboard[backlight-enabled]` being false does not blank the framebuffer, it
only turns the backlight off. Screenshots still work with the physical display
dark, which is handy: you can capture without lighting the scooter up.

A hand-started instance holds the display until you kill it. If a screenshot
comes back as the old content, check that only one scootui-qt is running.

Always `sync` before `lsc dbc off`. The DBC's root filesystem is rw but the
power cut is not graceful, so unsynced writes are lost.
