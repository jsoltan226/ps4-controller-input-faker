# PS4 Controller input faker
This is a little linux daemon that listens for active PS4 controller (Dualshock 4) input. When it detects something (a button press) it sends a fake keyboard event (for now `KEY_F21`) to the input subsystem.

## Why?
Every time I was playing a game that required a controller (i.e. Bloodborne), my screen would lock after a couple of minutes due to inactivity.
The reason for this is that `libinput` doesn't provide an interface for joysticks at all, which means that Wayland compostor that utilize it for input handling (pretty much all of them) won't notice that anything is happening unless I move the mouse or press a key on the keyboard etc.
A workaround for this would be to disable the lock timeout altogether while playing and then re-enable it once I'm done, but that was getting really annoying, so I decided to make a more covenient solution - whenever I press a button on the controller, the system thinks I also pressed a key on the keyboard.

## Dependencies
`libudev` and linux kernel headers.

## Installation
*Please don't install it yet* - right now it's a very hacky proof of concept and it's not guaranteed to work on your system at all.

But if you really want to, you can just clone the repo and, with `make` and a C compiler installed, build the project by running `make` in the repo directory.
This will build the debug version.
The output executable will be `bin/main`.
If you want a release build (Optimizations + LTO + strip + no debug logging) run `make release`.
If you want this thing to run at system startup, copy `bin/main` to `/usr/local/bin/ps4-controller-input-faker` and install the file `ps4-controller-input-faker.service` to `/etc/systemd/system/` or whatever your distro's equivalent is, end enable it with `systemctl enable --now ps4-controller-input-faker.service`.
To clean up the build files, run `make clean`.
