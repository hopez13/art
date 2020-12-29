#!/usr/bin/python3
#
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# This script can get info out of dexfiles using libdexfile_external
#

from abc import ABC
from ctypes import *
import os.path
import zipfile

libdexfile_external = CDLL(os.path.expandvars("$ANDROID_HOST_OUT/lib64/libdexfile_external.so"))

DexFileStr = c_void_p
ExtDexFile = c_void_p
class ExtMethodInfo(Structure):
  _fields_ = [("offset", c_int32), ("len", c_int32), ("name", DexFileStr)]

AllMethodsCallback = CFUNCTYPE(c_int, POINTER(ExtMethodInfo), c_void_p)
libdexfile_external.ExtDexFileOpenFromFd.argtypes = [c_int, c_size_t, c_char_p, POINTER(DexFileStr), POINTER(ExtDexFile)]
libdexfile_external.ExtDexFileOpenFromFd.restype = c_int
libdexfile_external.ExtDexFileOpenFromMemory.argtypes = [c_void_p, POINTER(c_size_t), c_char_p, POINTER(DexFileStr), POINTER(ExtDexFile)]
libdexfile_external.ExtDexFileOpenFromMemory.restype = c_int
libdexfile_external.ExtDexFileFree.argtypes = [ExtDexFile]
libdexfile_external.ExtDexFileGetAllMethodInfos.argtypes = [ExtDexFile, c_int, AllMethodsCallback, c_void_p]
libdexfile_external.ExtDexFileGetString.argtypes = [DexFileStr, POINTER(c_size_t)]
libdexfile_external.ExtDexFileGetString.restype= c_char_p
libdexfile_external.ExtDexFileFreeString.argtypes= [DexFileStr]

class DexClass(object):
  def __init__(self, name):
    self.name = name.strip()
    self.arrays = name.count('[')
    self.base_name = self.name if self.arrays == 0 else self.name[:self.arrays * 2 - 1]

  def __repr__(self):
    return self.name
  def _get_descriptor(self):
    if self.base_name == "int":
      return "["*self.arrays + "I"
    elif self.base_name == "short":
      return "["*self.arrays + "S"
    elif self.base_name == "long":
      return "["*self.arrays + "J"
    elif self.base_name == "char":
      return "["*self.arrays + "C"
    elif self.base_name == "boolean":
      return "["*self.arrays + "Z"
    elif self.base_name == "byte":
      return "["*self.arrays + "B"
    elif self.base_name == "float":
      return "["*self.arrays + "F"
    elif self.base_name == "double":
      return "["*self.arrays + "D"
    elif self.base_name == "void":
      return "["*self.arrays + "V"
    else:
      return "["*self.arrays + "L{};".format("/".join(self.base_name.split(".")))

  descriptor = property(_get_descriptor)

class Method(object):
  def __init__(self, mi):
    self.offset = mi.offset
    self.len = mi.len
    self.name = libdexfile_external.ExtDexFileGetString(mi.name, byref(c_size_t(0))).decode("utf-8")
    libdexfile_external.ExtDexFileFreeString(mi.name)

  def __repr__(self):
    return "(" + self.name + ")"

  def _get_descriptor(self):
    ret = DexClass(self.name.split(' ')[0])
    non_ret = self.name[len(ret.name) + 1:]
    arg_str = non_ret[non_ret.find("(") + 1:-1]
    args = [] if arg_str == "" else map(lambda a: DexClass(a.strip()).descriptor, arg_str.split(','))
    class_and_meth = non_ret[0:non_ret.find("(")]
    class_only = DexClass(class_and_meth[0:class_and_meth.rfind('.')])
    meth = class_and_meth[class_and_meth.rfind('.') + 1:]
    return "{cls}->{meth}({args}){ret}".format(cls = class_only.descriptor, meth = meth, args = ''.join(args), ret = ret.descriptor)

  def _get_name_only(self):
    ret = DexClass(self.name.split(' ')[0])
    non_ret = self.name[len(ret.name) + 1:]
    class_and_meth = non_ret[0:non_ret.find("(")]
    return class_and_meth

  def _get_class(self):
    ret = DexClass(self.name.split(' ')[0])
    non_ret = self.name[len(ret.name) + 1:]
    class_and_meth = non_ret[0:non_ret.find("(")]
    return DexClass(class_and_meth[0:class_and_meth.rfind('.')])

  name_only = property(_get_name_only)
  descriptor = property(_get_descriptor)
  klass = property(_get_class)

def real_get_methods_(dex_file_p):
  meths = []
  @AllMethodsCallback
  def my_cb(info, _):
    meths.append(Method(info[0]))
    return 0
  libdexfile_external.ExtDexFileGetAllMethodInfos(dex_file_p, c_int(1), my_cb, c_void_p())
  return meths

class BaseDexFile(ABC):
  def __init__(self):
    self.ext_dex_file_ = None
    return

  def GetMethods(self):
    return real_get_methods_(self.ext_dex_file_)

class FdDexFile(BaseDexFile):
  def __init__(self, fd, loc):
    super().__init__()
    res_fle_ptr = pointer(c_void_p())
    err_ptr = pointer(c_void_p())
    res = libdexfile_external.ExtDexFileOpenFromFd(c_int(fd),
                                                   0,
                                                   create_string_buffer(bytes(loc, "utf-8")),
                                                   err_ptr,
                                                   res_fle_ptr)
    if res == 0:
      err = libdexfile_external.ExtDexFileGetString(err_ptr.contents, byref(c_size_t()))
      out = Exception("Failed to open file: {}. Error was: {}".format(loc, err))
      libdexfile_external.ExtDexFileFreeString(err_ptr.contents)
      raise out
    self.ext_dex_file_ = res_fle_ptr.contents

class FileDexFile(FdDexFile):
  def __init__(self, file, loc):
    if type(file) == str:
      super().__init__(open(file, 'rb').fileno(), loc)
    else:
      super().__init__(file.fileno(), loc)

class MemDexFile(BaseDexFile):
  def __init__(self, dat, loc):
    assert type(dat) == bytes
    super().__init__()
    # Don't want GC to screw us over.
    self.mem_ref = (c_byte * len(dat)).from_buffer_copy(dat)
    res_fle_ptr = pointer(c_void_p())
    err_ptr = pointer(c_void_p())
    res = libdexfile_external.ExtDexFileOpenFromMemory(self.mem_ref,
                                                       byref(c_size_t(len(dat))),
                                                       create_string_buffer(bytes(loc, "utf-8")),
                                                       err_ptr,
                                                       res_fle_ptr)
    if res == 0:
      err = libdexfile_external.ExtDexFileGetString(err_ptr.contents, byref(c_size_t()))
      out = Exception("Failed to open file: {}. Error was: {}".format(loc, err))
      libdexfile_external.ExtDexFileFreeString(err_ptr.contents)
      raise out
    self.ext_dex_file_ = res_fle_ptr.contents

def OpenJar(fle):
  res = []
  with zipfile.ZipFile(fle) as zf:
    for f in zf.namelist():
      if f.endswith(".dex") and f.startswith("classes"):
        res.append(MemDexFile(zf.read(f), "classes" if type(fle) != str else fle))
  return res