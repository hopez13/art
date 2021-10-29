#!/usr/bin/env python3
"""
To compile the protobuf:
`protoc -I. --python_out=. perfetto_trace.proto`

Takes a perfetto trace with a java heap dump (from a hacked version of the code) and produces multiple files, one per pid.
The outputs are not complete traces, they only have a single HeapGraph object and don't have other perfetto metadata.
"""

import sys
import perfetto_trace_pb2

from collections.abc import Sequence

def main(argv: Sequence[str]) -> None:
  if len(argv) != 2:
    raise Exception("Wrong arguments")

  msg = perfetto_trace_pb2.Trace()
  with open(argv[1], 'rb') as f:
    msg.ParseFromString(f.read())

  outputs = {}
  for packet in msg.packet:
    if not packet.HasField("heap_graph"):
      continue
    source = packet.heap_graph

    if not (source.pid in outputs):
      outputs[source.pid] = perfetto_trace_pb2.HeapGraph()
      outputs[source.pid].CopyFrom(source)
      continue

    output = outputs[source.pid]
    assert(output.continued)
    assert(output.index == source.index - 1)
    output.objects.extend(source.objects)
    output.types.extend(source.types)
    output.field_names.extend(source.field_names)
    output.location_names.extend(source.location_names)
    output.continued = source.continued
    output.index = source.index

  for pid, hg in outputs.items():
    msg = perfetto_trace_pb2.Trace()
    packet = msg.packet.add()
    packet.heap_graph.CopyFrom(hg)

    with open("result_{}.pb".format(pid), "wb") as f:
      f.write(msg.SerializeToString())

if __name__ == '__main__':
  main(sys.argv)
