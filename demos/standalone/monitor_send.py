#!/usr/bin/env python3

import serial
import sys
import time
import os

def send_commands(ser,data_fn):
  ts_start = time.time()
  nbytes = 0
  with open(data_fn, 'r') as data_fh:
    alldata = b''
    n = 0
    for l in data_fh:
      ts, data = l.strip().split(',', 1)
      data = b''.join([int(x,0).to_bytes(1, 'little') for x in data.split(',') if x])
      alldata = b''.join([alldata,data])
      n = n + 1
      if n == 1024:
        ser.write(alldata)
        nbytes += len(alldata)
        alldata = b''
        n = 0
    ser.write(alldata)
    nbytes += len(alldata)

  print(f'sent {nbytes:d} bytes in {(time.time() - ts_start):.1f}  seconds.')


def main(argv0, dev, data_fn=''):

  ser = serial.Serial(dev, 115200)

  ser.write(b'\x00\xaa')
  send_commands(ser,'data-lcd-init.txt')

  data_fn = 'frame.nfo'

  while not os.path.exists(data_fn):
    time.sleep(0.1)

  while True:
    send_commands(ser,data_fn)
    last = os.stat(data_fn).st_mtime
    while True:
      if (os.stat(data_fn).st_mtime != last):
        time.sleep(0.2)  # seems file might not be written yet otherwise?
        break
      else:
        time.sleep(0.1)

  ser.close()

  return 0

if __name__ == '__main__':
  sys.exit(main(*sys.argv))
