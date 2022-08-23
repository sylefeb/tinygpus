#!/usr/bin/env python3

import serial
import sys
import time
import os
import curses
from pynput import keyboard

ser = serial.Serial('COM3', 115200)

def on_press(key):
  try:
    ser.write(bytes(key.char,'utf-8'))
  except AttributeError:
    print('special key {0} pressed'.format(
        key))

def on_release(key):
  ser.write(b'\x00')
  print('{0} released'.format(
    key))
  if key == keyboard.Key.esc:
    # Stop listener
    return False

def main(argv0, dev, data_fn=''):

  stdscr = curses.initscr()
  curses.noecho()
  curses.cbreak()
  stdscr.keypad(True)
  
  ser.write(b'\x00')
  
  with keyboard.Listener(
        on_press=on_press,
        on_release=on_release) as listener:
    listener.join()

#      ser.write(c.to_bytes(1,'big'))
  
  ser.close()

  curses.nocbreak()
  stdscr.keypad(False)
  curses.echo()
  curses.endwin()
  
  return 0

if __name__ == '__main__':
  sys.exit(main(*sys.argv))
