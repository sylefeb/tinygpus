#!/usr/bin/env python3

import serial
import sys
import time
import os
import curses
from pynput import keyboard

active = 1
ser    = serial.Serial('COM3', 115200, timeout=0)

def on_press(key):
  global ser
  try:
    ser.write(bytes(key.char,'utf-8'))
  except AttributeError:
    print('special key {0} pressed'.format(key))

def on_release(key):
  global active,ser
  ser.write(b'\x00')
  print('{0} released'.format(key))
  if key == keyboard.Key.esc:
    print('active ',active)
    active = 0

def main(argv0, dev, data_fn=''):

  stdscr = curses.initscr()
  curses.noecho()
  curses.cbreak()
  stdscr.keypad(True)
  stdscr.nodelay(1)
  
  ser.write(b'\x00')
  
  listener = keyboard.Listener(
      on_press=on_press,
      on_release=on_release)
  listener.start()
  
  stdscr.clear()
  
  str = ''
  while active == 1:
    by = ser.read()
    str = str + by.decode('utf-8')
    if len(str) > 0:
      if str[-1] == '\n':
        print(str)
        stdscr.addstr(0, 0, str)
        stdscr.refresh()
        str = ''
  
  listener.stop()
  ser.close()

  curses.nocbreak()
  stdscr.keypad(False)
  curses.echo()
  curses.endwin()
  
  return 0

if __name__ == '__main__':
  sys.exit(main(*sys.argv))
