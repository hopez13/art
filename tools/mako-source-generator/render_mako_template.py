#!/usr/bin/python3.4

#
# Copyright (C) 2016 The Android Open Source Project
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

# System imports
from io import StringIO
import os
import sys

# Mako imports
from mako.lookup import TemplateLookup
from mako.runtime import Context
from mako.template import Template

# Local imports
import mako_helpers

_CURRENT_DIR=os.path.dirname(os.path.realpath(__file__))

def eprint(*args, **kwargs):
  """
  Print any arguments to stderr.
  """
  print(*args, file=sys.stderr, **kwargs)

def render(template_name, output_name=None):
  """
  Render a Mako file by processing its template and emitting the rendered string.

  The template automatically imports all the public attributes from the mako_helpers module.

  The output file is encoded with UTF-8.

  Args:
    template_name: path to a Mako template file
    output_name: path to the output file, or None to use stdout
  """

  # Share the string buffer Mako templates are rendered into
  # with the mako_helpers module. Used to change partially rendered data.
  # Normally Mako only lets you emit new strings for a call but this lets us do in-place edits.
  buf = StringIO()
  mako_helpers._context_buf = buf

  # Automatically "import" everything from metadata_helpers module into the mako templates.
  helpers = [(i, getattr(mako_helpers, i))
              for i in dir(mako_helpers) if not i.startswith('_')]
  helpers = dict(helpers)

  lookup = TemplateLookup(directories=_CURRENT_DIR)
  tpl = Template(filename=template_name, lookup=lookup)

  ctx = Context(buf, **helpers)
  tpl.render_context(ctx)

  tpl_data = buf.getvalue()
  mako_helpers._context_buf = None
  buf.close()

  if output_name is None:
    print(tpl_data, end="")
  else:
    open(output_name, "wb").write(tpl_data.encode('utf-8'))

#####################
#####################

if __name__ == "__main__":
  if len(sys.argv) <= 1:
    eprint("Usage: %s <mako-template.mako> [output-file-name]" % (sys.argv[0]))
    sys.exit(0)

  file_name = sys.argv[1]

  if not os.path.isfile(file_name):
      eprint("ERROR: File %s does not exist" %(file_name))
      sys.exit(1)

  if len(sys.argv) > 2:
    output_name = sys.argv[2]
  else:
    output_name = None

  render(file_name, output_name)

# vim: ts=2 sw=2 et
