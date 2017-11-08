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
A python program that simulates the plugin side of the dt_fds transport for testing.
"""

import argparse
import array
import ctypes
import contextlib
import tempfile
import concurrent.futures as futures
import socket
import subprocess
import os

FINISHED = False

libc = ctypes.cdll.LoadLibrary("libc.so.6")

def eventfd(init_val, flags):
  return libc.eventfd(init_val, flags)

def HandleInvoke(cmd_pre, cmd_post, jdwp_lib, jdwp_ops, remote_sock):
  full_cmd = list(cmd_pre)
  full_cmd.append("-agentpath:" + jdwp_lib + "=" + jdwp_ops + "transport=dt_fd_forward,address=" +
                  str(remote_sock.fileno()))
  full_cmd += cmd_post
  print("Running " + str(full_cmd))
  # Start the actual process with the fd being passed down.
  subprocess.run(full_cmd, close_fds=False)

@contextlib.contextmanager
def make_pipe():
  (rfd, wfd) = os.pipe()
  # Only the read-end needs to be sent down.
  os.set_inheritable(rfd, True)
  yield (rfd, wfd)
  os.close(rfd)
  os.close(wfd)

@contextlib.contextmanager
def make_eventfd():
  fd = eventfd(1, 0)
  yield fd
  os.close(fd)

def send_fds(sock, remote_read, remote_write, remote_event):
  sock.sendmsg([b"!"], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [remote_read,
                                                                                 remote_write,
                                                                                 remote_event]))])


def HandleSockets(host, port, local_sock):
  with socket.socket() as sock:
    with make_eventfd() as event_fd:
      addr = host + ":" + str(port)
      print("\n\nListening on " + str((host, port)) + "\n\n")
      sock.bind((host, port))
      print("\n\nSocket bound\n\n")
      sock.listen()
      print("\n\nSocket listening\n\n")
      (conn, addr) = sock.accept()
      print("connection accepted from " + str(addr))
      send_fds(local_sock, conn.fileno(), conn.fileno(), event_fd)
      # TODO actually act as a proxy.

@contextlib.contextmanager
def make_sockets():
  (rfd, lfd) = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
  # Only the read-end needs to be sent down.
  os.set_inheritable(rfd.fileno(), True)
  yield (rfd, lfd)
  rfd.close()
  lfd.close()

def main():
  global FINISHED

  parser = argparse.ArgumentParser(description="Runs a socket that forwards to dt_fds.",
                                   prefix_chars="+")
  parser.add_argument("++host", type=str, default="localhost",
                      help="Host we will listen for traffic on. Defaults to localhost.")
  parser.add_argument("++debug-lib", type=str, default="libjdwp.so",
                      help="jdwp library we pass to -agentlib:")
  parser.add_argument("++debug-options", type=str, default="server=y,suspend=y,",
                      help="non-address options we pass to jdwp agent, default server=y,suspend=y,")
  parser.add_argument("++port", type=int, default=12345,
                      help="port we will expose the traffic on. Defaults to 12345.")
  parser.add_argument("++pre-end", type=int, default=1,
                      help="number of 'rest' arguments to put before passing in the debug options")
  parser.add_argument("rest", nargs="*", help="What we run with a debugger!")
  args = parser.parse_args()

  with futures.ThreadPoolExecutor(max_workers = 2) as tp:
    with make_sockets() as (remote_sock, local_sock):
      # socket_handler = tp.submit(HandleSockets, args.host, args.port, local_sock)
      HandleSockets(args.host, args.port, local_sock)
      invoker = tp.submit(HandleInvoke,
                          args.rest[:args.pre_end],
                          args.rest[args.pre_end:],
                          args.debug_lib,
                          args.debug_options,
                          remote_sock)
      invoker.result()
      FINISHED = True
      # socket_handler.result()

if __name__ == '__main__':
  main()
