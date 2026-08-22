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

## Finding what keeps the UI rendering

Qt Quick renders every frame for as long as any animation is running, whether
or not the result is visible. One `loops: Animation.Infinite` behind an
invisible item, or behind a backlight that has been switched off, pins the DBC
at 59 Hz and costs about a quarter of a core for nothing.

The symptom is easy to confirm and easy to misread. `QSG_RENDER_TIMING=1` shows
frames arriving every 15 to 16 ms while the renderer reports no work at all:

```
polishAndSync: start, elapsed since last call: 16 ms
time in renderer: total=0ms, preprocess=0, updates=0, rendering=0
```

Frames with `updates=0, rendering=0` mean the scene graph has nothing to draw
and something is still asking for a new frame. That is an animation, not a
dirty item, so looking for what changed on screen will not find it.

`SCOOTUI_DUMP_ANIMATIONS=1` lists every running animation every three seconds,
with the QML file it came from:

```
$ SCOOTUI_DUMP_ANIMATIONS=1 SCOOTUI_REDIS_HOST=127.0.0.1:6399 ./build/bin/scootui
RUNNING-ANIM QQuickRotationAnimation qrc:/ScootUI/qml/screens/MaintenanceScreen.qml
RUNNING-ANIM-TOTAL 1
```

Run it against a scratch Redis seeded with the state you care about, since what
is running depends entirely on which screens and overlays are live. On the
target the same variable works, but it is usually quicker to reproduce the
state locally.

When you find one, the fix is almost never to stop the animation outright. It
is to give `running:` a condition that means "someone can actually see this".
Item visibility is not that condition: an item can be visible in the scene
while the panel it is drawn on is switched off.

## What a frame costs

Frames cost about the same whatever is in them. The scene graph reports its own
work, and on this UI it is consistently nothing:

```
time in renderer: total=0ms, preprocess=0, updates=0, rendering=0
```

All of the cost is the polish, sync, swap cycle around it. That makes CPU close
to linear in frames per second and almost independent of what is drawn. Measured
on the desktop build, one screen, three update rates:

| frames/10s | CPU |
|---|---|
| 627 | 18% |
| 245 | 8% |
| 122 | 4% |

On the DBC the same relationship holds at roughly half a percent of a core per
frame per second, over a floor of about 3%.

Two things follow. Making a widget visually simpler buys you nothing. Making it
update less often buys you everything, in direct proportion.

It also means `RotationAnimator` and friends are a trap here. Animators drive
the render thread rather than the property system, and left to themselves they
run as fast as the machine allows instead of at the display rate: swapping one
`RotationAnimation` for a `RotationAnimator` took the desktop build to 1325 fps
and 278% of a core.

### The global frame cap we did not take

Capping the whole UI at 30 Hz is possible and was considered.

`QT_QPA_EGLFS_SWAPINTERVAL` is not the lever it looks like. `eglSwapBuffers`
does not block on etnaviv/imx-drm, so a swap interval has nothing to act on;
what paces rendering is the page-flip wait in the eglfs_kms backend under
`QT_QPA_EGLFS_KMS_ATOMIC=1`. Setting it to `2` would very likely do nothing.

That leaves one mechanism: a custom `QAnimationDriver` advancing
`QUnifiedTimer` on a 30 Hz timer. It is narrower than it sounds, throttling
animations but not repaints caused by data changes marking items dirty.

It is not being done, for two reasons. The costs are real (map panning is where
30 Hz shows, input gains up to 16 ms, and presentation quantises to vblank
multiples so an overrunning frame drops to 19.7 Hz rather than degrading
gently). More importantly, a cap rations waste instead of removing it. Both
render-cost problems found so far were work done for something nobody could see,
a spinner animating against a blank panel and a debug overlay instantiated
permanently while switched off. A global cap would have halved the price of both
and left them in place. Look for the work first; a cap is what you reach for
when there is no waste left to find.

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
