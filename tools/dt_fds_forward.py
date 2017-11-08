#!/usr/bin/env python3
#
# Copyright 2017, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
A python program that simulates the plugin side of the dt_fd_forward transport for testing.

This program will invoke a given java language runtime program and send down debugging arguments
that cause it to use the dt_fd_forward transport. This will then create a normal server-port that
debuggers can attach to.
"""

import argparse
import array
import concurrent.futures as futures
import contextlib
import ctypes
import os
import select
import socket
import subprocess
import sys

FINISHED = False

LISTEN_START_MESSAGE = b"dt_fd_forward:START-LISTEN\x00"
LISTEN_END_MESSAGE   = b"dt_fd_forward:END-LISTEN\x00"

libc = ctypes.cdll.LoadLibrary("libc.so.6")
def eventfd(init_val, flags):
  """
  Creates an eventfd. See 'man 2 eventfd' for more information.
  """
  return libc.eventfd(init_val, flags)

@contextlib.contextmanager
def make_eventfd(init):
  """
  Creates an eventfd with given initial value that is closed after the manager finishes.
  """
  fd = eventfd(init, 0)
  yield fd
  os.close(fd)

@contextlib.contextmanager
def make_sockets():
  """
  Make a (remote,local) socket pair. The remote socket is inheritable by forked processes. They are
  both linked together.
  """
  (rfd, lfd) = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
  # Only the read-end needs to be sent down.
  os.set_inheritable(rfd.fileno(), True)
  yield (rfd, lfd)
  rfd.close()
  lfd.close()

def send_fds(sock, remote_read, remote_write, remote_event):
  """
  Send the three fds over the given socket.
  """
  sock.sendmsg([b"!"],  # We don't actually care what we send here.
               [(socket.SOL_SOCKET,  # Send over socket.
                 socket.SCM_RIGHTS,  # Payload is file-descriptor array
                 array.array('i', [remote_read, remote_write, remote_event]))])


def HandleSockets(host, port, local_sock, finish_event):
  """
  Handle the IO between the network and the runtime.
  """
  global FINISHED
  sock = None
  while not FINISHED:
    sources = [local_sock, finish_event] + ([sock] if sock is not None else [])
    print("Starting select on " + str(sources))
    (rf, _, _) = select.select(sources, [], [])
    if local_sock in rf:
      buf = local_sock.recv(1024)
      print("Local_sock has data: " + str(buf))
      if buf == LISTEN_START_MESSAGE:
        sock = socket.socket()
        sock.bind((host, port))
        sock.listen()
        print("listening on " + str(sock))
      elif buf == LISTEN_END_MESSAGE:
        print("End listening")
        if sock is not None:
          sock.close()
          sock = None
        else:
          print("ignoring extra stop-listening.")
      else:
        print("Unknown data received from socket " + str(buf))
        return
    elif sock is not None and sock in rf:
      (conn, addr) = sock.accept()
      print("connection accepted from " + str(addr) + " sending fds to target.")
      with make_eventfd(1) as efd:
        send_fds(local_sock, conn.fileno(), conn.fileno(), efd)
    if finish_event in rf:
      print("woke up from finish_event")
      assert FINISHED
  if sock is not None:
    sock.close()

def StartChildProcess(cmd_pre, cmd_post, jdwp_lib, jdwp_ops, remote_sock, can_be_runtest):
  """
  Open the child java-language runtime process.
  """
  full_cmd = list(cmd_pre)
  jdwp_arg = jdwp_lib + "=" + \
             jdwp_ops + "transport=dt_fd_forward,address=" + str(remote_sock.fileno())
  if can_be_runtest and cmd_pre[0].endswith("run-test"):
    print("Assuming run-test. Pass --no-run-test if this isn't true")
    full_cmd += ["--with-agent", jdwp_arg]
  else:
    full_cmd.append("-agentpath:" + jdwp_arg)
  full_cmd += cmd_post
  print("Running " + str(full_cmd))
  # Start the actual process with the fd being passed down.
  return subprocess.Popen(full_cmd, close_fds=False)

def main():
  global FINISHED
  parser = argparse.ArgumentParser(description="""
                                   Runs a socket that forwards to dt_fds.

                                   Pass '--' to start passing in the program we will pass the debug
                                   options down to.
                                   """)
  parser.add_argument("--host", type=str, default="localhost",
                      help="Host we will listen for traffic on. Defaults is 'localhost'.")
  parser.add_argument("--debug-lib", type=str, default="libjdwp.so",
                      help="jdwp library we pass to -agentpath:. Default is 'libjdwp.so'")
  parser.add_argument("--debug-options", type=str, default="server=y,suspend=y,",
                      help="non-address options we pass to jdwp agent, default is " +
                           "'server=y,suspend=y,'")
  parser.add_argument("--port", type=int, default=12345,
                      help="port we will expose the traffic on. Defaults to 12345.")
  parser.add_argument("--no-run-test", default=False, action="store_true",
                      help="don't pass in arguments for run-test even if it looks like that is " +
                           "the program")
  parser.add_argument("--pre-end", type=int, default=1,
                      help="number of 'rest' arguments to put before passing in the debug options")
  end_idx = 0 if '--' not in sys.argv else sys.argv.index('--')
  if end_idx == 0 and ('--help' in sys.argv or '-h' in sys.argv):
    parser.print_help()
    return
  args = parser.parse_args(sys.argv[:end_idx][1:])
  rest = sys.argv[1 + end_idx:]

  with futures.ThreadPoolExecutor(max_workers = 2) as tp:
    with make_eventfd(0) as wakeup_event:
      with make_sockets() as (remote_sock, local_sock):
        invoker = StartChildProcess(rest[:args.pre_end],
                                    rest[args.pre_end:],
                                    args.debug_lib,
                                    args.debug_options,
                                    remote_sock,
                                    not args.no_run_test)
        socket_handler = tp.submit(HandleSockets, args.host, args.port, local_sock, wakeup_event)
        # TODO It would be nice not to busy-loop here but that would require passing sockets to
        # other ProcessPoolExecutor processes which is rather annoying.
        while invoker.poll() is None:
          pass
        FINISHED = True
        # Write any 64 bit value to the wakeup_event to make sure that the socket handler will wake
        # up. This is just the uint64_t 2.
        os.write(wakeup_event, b'x00x00x00x00x00x00x01x00')
        socket_handler.result()

if __name__ == '__main__':
  main()
