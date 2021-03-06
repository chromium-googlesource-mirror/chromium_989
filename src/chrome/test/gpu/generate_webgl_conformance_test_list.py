#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Auto-generates the WebGL conformance test list header file.

Parses the WebGL conformance test *.txt file, which contains a list of URLs
for individual conformance tests (each on a new line). It recursively parses
*.txt files. For each test URL, the matching gtest call is created and
sent to the C++ header file.
"""

import getopt
import os
import re
import sys

COPYRIGHT = """\
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""
WARNING = """\
// DO NOT EDIT! This file is auto-generated by
//   generate_webgl_conformance_test_list.py
// It is included by webgl_conformance_tests.cc

"""
HEADER_GUARD = """\
#ifndef CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_
#define CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_

"""
HEADER_GUARD_END = """
#endif  // CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_

"""

# Assume this script is run from the src/chrome/test/gpu directory.
INPUT_DIR = "../../../third_party/webgl_conformance"
INPUT_FILE = "00_test_list.txt"
OUTPUT_FILE = "webgl_conformance_test_list_autogen.h"
EXPECTATION_FILE = "webgl_conformance_test_expectations.txt"
EXPECTATION_REGEXP = re.compile(
    r'^(?P<BUG>\S+)\s+'
     '(?P<MODIFIER>(\s*(WIN|MAC|LINUX|RELEASE|DEBUG)\s*)+):'
     '(?P<TEST>[^=]+)='
     '(?P<OUTCOME>(\s*(PASS|FAIL|TIMEOUT)\s*)+)')

def map_to_macro_conditions(modifier_list):
  """Returns a string containing macro conditions wrapped in '(*)'.

  Given a list containing 'WIN', 'MAC', 'LINUX', 'RELEASE', or 'DEBUG',
  return the corresponding macro conditions.
  """
  rt = ''
  release = False
  debug = False
  for modifier in modifier_list:
    if modifier == 'RELEASE':
      release = True
    elif modifier == 'DEBUG':
      debug = True
    else:
      if rt:
        rt += ' || '
      if modifier == 'WIN':
        rt = rt + 'defined(OS_WIN)'
      elif modifier == 'MAC':
        rt = rt + 'defined(OS_MACOSX)'
      elif modifier == 'LINUX':
        rt = rt + 'defined(OS_LINUX)'

  if release == debug:
    return rt

  if rt:
    rt = '(' + rt + ') && '

  if debug:
    rt = rt + '!defined(NDEBUG)'
  if release:
    rt = rt + 'defined(NDEBUG)'

  return rt

def main(argv):
  """Main function for the WebGL conformance test list generator.
  """
  if not os.path.exists(os.path.join(INPUT_DIR, INPUT_FILE)):
    print >> sys.stderr, "ERROR: WebGL conformance tests do not exist."
    print >> sys.stderr, "Run the script from the directory containing it."
    return 1
  if not os.path.exists(EXPECTATION_FILE):
    print >> sys.stderr, "ERROR: test expectations file does not exist."
    print >> sys.stderr, "Run the script from the directory containing it."
    return 1

  output = open(OUTPUT_FILE, "w")
  output.write(COPYRIGHT)
  output.write(WARNING)
  output.write(HEADER_GUARD)

  test_prefix = {}
  test_expectations = open(EXPECTATION_FILE)
  for line in test_expectations:
    line_match = EXPECTATION_REGEXP.match(line)
    if line_match:
      match_dict = line_match.groupdict()
      modifier_list = match_dict['MODIFIER'].strip().split()
      macro_conditions = map_to_macro_conditions(modifier_list)
      test = match_dict['TEST'].strip()
      outcome_list = match_dict['OUTCOME'].strip().split()
      if 'TIMEOUT' in outcome_list:
        prefix = "DISABLED_"
      elif 'FAIL' in outcome_list:
        if 'PASS' in outcome_list:
          prefix = "FLAKY_"
        else:
          prefix = "FAILS_"
      if macro_conditions:
        output.write('#if %s\n' % macro_conditions)
        output.write('#define MAYBE_%s %s%s\n' % (test, prefix, test))
        output.write('#elif !defined(MAYBE_%s)\n' % test)
        output.write('#define MAYBE_%s %s\n' % (test, test))
        output.write('#endif\n')
        test_prefix[test] = 'MAYBE_'
      else:
        test_prefix[test] = prefix
  test_expectations.close()

  unparsed_files = [INPUT_FILE]
  while unparsed_files:
    filename = unparsed_files.pop(0)
    try:
      input = open(os.path.join(INPUT_DIR, filename))
    except IOError:
      print >> sys.stderr, "WARNING: %s does not exist (skipped)." % filename
      continue

    for url in input:
      url = re.sub("//.*", "", url)
      url = re.sub("#.*", "", url)
      url = re.sub("\s+", "", url)

      if not url:
        continue

      # Cannot use os.path.join() because Windows with use "\\" but this path
      # is sent through javascript.
      if os.path.dirname(filename):
        url = "%s/%s" % (os.path.dirname(filename), url)

      # Queue all text files for parsing, because test list URLs are nested
      # through .txt files.
      if re.match(".+00_test_list\.txt\s*$", url):
        unparsed_files.append(url)

      # Convert the filename to a valid test name and output the gtest code.
      else:
        name = os.path.splitext(url)[0]
        name = re.sub("\W+", "_", name)
        if os.path.exists(os.path.join(INPUT_DIR, url)):
          # Append "DISABLED_" or "FAILS_" if needed.
          if name in test_prefix:
            name = test_prefix[name] + name
          output.write('CONFORMANCE_TEST(%s,\n  "%s");\n' % (name, url))
        else:
          print >> sys.stderr, "WARNING: %s does not exist (skipped)." % url
    input.close()

  output.write(HEADER_GUARD_END)
  output.close()
  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
