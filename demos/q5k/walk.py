#!/usr/bin/env python3

import serial
import sys
import time
import os
import curses
from pynput import keyboard

active  = 1
command = 0

def on_press(key):
  global ser,command
  if key == keyboard.Key.up:
    command = command | 1
  if key == keyboard.Key.down:
    command = command | 2
  if key == keyboard.Key.right:
    command = command | 4
  if key == keyboard.Key.left:
    command = command | 8
  if key == keyboard.Key.page_up:
    command = command | 16
  if key == keyboard.Key.page_down:
    command = command | 32
  if key == keyboard.Key.home:
    command = command | 64
  if key == keyboard.Key.end:
    command = command | 128
  ser.write(command.to_bytes(1,'big'))

def on_release(key):
  global active,ser,command
  print('{0} released'.format(key))
  if key == keyboard.Key.esc:
    print('active ',active)
    active = 0
    return False
  if key == keyboard.Key.up:
    command = command & (~1)
  if key == keyboard.Key.down:
    command = command & (~2)
  if key == keyboard.Key.right:
    command = command & (~4)
  if key == keyboard.Key.left:
    command = command & (~8)
  if key == keyboard.Key.page_up:
    command = command & (~16)
  if key == keyboard.Key.page_down:
    command = command & (~32)
  if key == keyboard.Key.home:
    command = command & (~64)
  if key == keyboard.Key.end:
    command = command & (~128)
  ser.write(command.to_bytes(1,'big'))

def main(argv0, dev, data_fn=''):
  global ser

  stdscr = curses.initscr()
  curses.noecho()
  curses.cbreak()
  stdscr.keypad(True)
  stdscr.nodelay(1)
  stdscr.clear()
 
  ser = serial.Serial(dev, 115200, timeout=0)

  ser.write(b'\x00')
  
  listener = keyboard.Listener(
      on_press=on_press,
      on_release=on_release)
  listener.start()
  
  
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
