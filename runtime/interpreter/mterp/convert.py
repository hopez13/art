#!/usr/bin/env python
import re
from os import listdir

def_re = re.compile(r'^%default {(.*)}', re.DOTALL)
inc_re = re.compile(r'^%include ".*/(.*).S" *{?(.*)}?', re.DOTALL)
arg_re = re.compile(r' *"([^"]*)" *: *"([^"]*)" *', re.DOTALL)
sym_re = re.compile(r'(?<!\$)\$([a-zA-Z]\w*|\{[a-zA-Z]\w*\})', re.DOTALL)

for arch in ["arm", "arm64", "mips", "mips64", "x86", "x86_64"]:
    for file in listdir(arch):
        f = open(arch + "/" + file, "r")
        lines = f.readlines()
        f.close()

        method_name = file.replace(".S", "")

        f = open(arch + "/" + file, "w")
        args = []
        match = def_re.match(lines[0])
        seen_arg_names = {"opcode":True, "opnum":True}
        if match:
            del lines[0]
            for arg in arg_re.findall(match.group(1)):
                args.append(arg[0] + '="' + arg[1] + '"')
                seen_arg_names[arg[0]] = True
        for sym in sym_re.findall("".join(lines)):
            sym = sym.strip('{}')
            if not seen_arg_names.has_key(sym) and method_name != "header" and method_name != "alt_stub":
                args.append(sym + '=""')
                seen_arg_names[sym] = True

        f.write("%def " + method_name + "(" + ", ".join(args) + "):\n")

        for line in lines:
            match = inc_re.match(line)
            if match:
                target = match.group(1)
                args = []
                for arg in arg_re.findall(match.group(2)):
                    args.append(arg[0] + '="' + arg[1] + '"')
                if target == "field":
                    args.append("helper=helper")
                f.write("%  " + target + "(" + ", ".join(args) + ")\n")
            elif line.startswith("%break"):
                f.write("%def " + file.replace(".S", "") + "_sister_code" + "():\n")
            else:
                f.write(line)
        f.truncate()
        f.close()

