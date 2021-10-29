#!/bin/sh
filename="$1"
protoc --decode=perfetto.protos.Trace -I . ./perfetto_trace.proto < ${filename} > ${filename%.*}.textproto
