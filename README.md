# Veikk A30 Drawing Tablet driver for Linux

v1.2

**Note**: Pen capabilities may also work on the Veikk A50 and S640 as it is identical to the A30, but the author of this repo does not own these devices to verify. Additional capabilities for other devices  (e.g., tablet buttons) have yet to be added.

A simple driver for the [Veikk A30 drawing tablet][0], using the `usbhid` HID API. This draws heavily off of the [Wacom driver][1], and is simplified to tailor to the A30x's capabilities. The configuration utility uses the `sysfs` interface.

The driver interfaces (absolute) cursor movement, pressure sensitivity, and the two stylus buttons. Full 32768x32768 cursor position sensitivity and 8192-level pressure sensitivity are included.

---

### Setup instructions

**Arch users**: thanks to [@jlam55555][6]

Run the install instructions, and the driver should be set up! Woo hoo!

Check out the [issues tab][5] for known setup issues and solutions.

---

### Install instructions

Make sure you have `make` and the appropriate linux headers installed (`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][3] for more details.

    make
    sudo make install clean

If you are getting a `Required key not available` error, please see [this issue][4].

---

### Uninstall instructions

    sudo make uninstall

---

### Configuration (very beta)

Run the configuration utility and follow the instructions to change a setting. Note that this is very beta, so input validation doesn't exist yet and data will not be saved after reloading the module or restarting. Those updates will come with the visual configuration utility.

    ./config.sh

Configuration options:

- Orientation:
    - `0`: no rotation (default) 
    - `1`: rotate right 90 deg
    - `2`: rotate 180 deg (left-handed mode)
    - `3`: rotate left 90 deg
- Screen mapping (all values in integers percents, e.g., `45`):
    - Left: left boundary (default: `0`)
    - Top: top boundary (default: `0`)
    - Right: right boundary (default: `100`)
    - Bottom: bottom boundary (default: `100`)
- Pressure mapping:
    - `0`: linear mapping (`p_out = p_raw`) (default)
    - `1`: constant function, half pressure (`p_out = p_max / 2`)
    - `2`: power function, `n=1/2` (`p_out = sqrt(p_raw)`)
    - `3`: power function, `n=2` (`p_out = p_raw^2`)
    - `4`: linear mapping, reduced input pressure (`p_out = min(4/3 * p_raw, p_max)`)
- Screen or Mouse:
    - `0`: The size of the working area of the graphics tablet matches the screen (default)
    - `1`: moving the pen works like a mouse

---


### Disclaimers

This was hastily tailored for the A30 *only* (the author of this package can only afford the cheapest tablet), but PRs for other [Veikk tablets][2] are very welcome. [It's reported that pen capabilities work for the A50 as well][5].

This is also my first Linux driver. If there are optimizations or conventions that would be optimal to follow, please let me know or create a PR.


[0]: https://www.veikk.com/product/40.html
[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
[2]: http://www.veikk.com/pen-tablet/
[3]: https://askubuntu.com/questions/554624/how-to-resolve-the-lib-modules-3-13-0-27-generic-build-no-such-file-or-direct
[4]: https://github.com/jlam55555/veikk-s640-driver/issues/3
[5]: https://github.com/jlam55555/veikk-s640-driver/pull/1
[6]: https://github.com/jlam55555
