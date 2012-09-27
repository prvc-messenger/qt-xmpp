#!/usr/bin/python

import getopt
import os
import platform
import sys

root = os.path.dirname(__file__)
report_path = None

def usage():
    print "Usage: run.py [options]"

# parse options
try:
    opts, args = getopt.getopt(sys.argv[1:], 'hx:')
except getopt.GetoptError, err:
    print err
    usage()
    sys.exit(2)

for opt, optarg in opts:
    if opt == '-h':
        usage()
        sys.exit()
    elif opt == '-x':
        report_path = optarg
        if not os.path.exists(report_path):
            os.mkdir(report_path)

# set library path
path = os.path.join(root, '..', 'src')
if platform.system() == 'Darwin':
    os.environ['DYLD_LIBRARY_PATH'] = path
else:
    os.environ['LD_LIBRARY_PATH'] = path

# run tests
if platform.system() == 'Darwin':
    prog = os.path.join(root, 'qxmpp-tests.app', 'Contents', 'MacOS', 'qxmpp-tests')
else:
    prog = os.path.join(root, 'qxmpp-tests.app')
if report_path:
    os.system('%s -xunitxml -o %s/%s.xml' % (prog, report_path, test))
else:
    os.system(prog)
