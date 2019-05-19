# batti

Simple battery status indicator for system tray, via D-Bus interface of "org.freedesktop.UPower" object.

Dependences: UPower https://upower.freedesktop.org/

Set your: BATTERY

    gcc -O2 -s `pkg-config --cflags --libs gtk+-3.0` -o batti batti.c
