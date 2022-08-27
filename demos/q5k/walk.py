#!/usr/bin/env python3

import serial
import sys
import time
import os
import curses
from pynput import keyboard

active  = 1
ser     = serial.Serial('COM3', 115200, timeout=0)
command = 0

def on_press(key):
  global ser,command
  try:
    if key.char == 'w':
      command = command | 1
    if key.char == 's':
      command = command | 2
    if key.char == 'd':
      command = command | 4
    if key.char == 'a':
      command = command | 8
    if key.char == 'e':
      command = command | 16
    if key.char == 'q':
      command = command | 32
    ser.write(command.to_bytes(1,'big'))
  except AttributeError:
    print('special key {0} pressed'.format(key))

def on_release(key):
  global active,ser,command
  print('{0} released'.format(key))
  if key == keyboard.Key.esc:
    print('active ',active)
    active = 0
    return False
  if key.char == 'w':
    command = command & (~1)
  if key.char == 's':
    command = command & (~2)
  if key.char == 'd':
    command = command & (~4)
  if key.char == 'a':
    command = command & (~8)
  if key.char == 'e':
    command = command & (~16)
  if key.char == 'q':
    command = command & (~32)
  ser.write(command.to_bytes(1,'big'))

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
        try:
          if ord(str[0]) - ord('0') < 10:
            stdscr.addstr(ord(str[0]) - ord('0'), 0, str[1:])
          else:
            stdscr.addstr(20, 0, str[1:])
          stdscr.refresh()
          str = ''
        except:
          print('error: ',str)
  
  listener.stop()
  ser.close()

  curses.nocbreak()
  stdscr.keypad(False)
  curses.echo()
  curses.endwin()
  
  return 0

if __name__ == '__main__':
  sys.exit(main(*sys.argv))
