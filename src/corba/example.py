#!/usr/bin/env python

# This is a simple example that demonstrates how to control xine from
# a Python script via the CORBA interface. This script requires the
# ORBit-Python module which can be downloaded from
#   http://orbit-python.sourceforge.net/

import CORBA
import posix
import sys

CORBA.load_idl("xine.idl")
orb = CORBA.ORB_init("orbit-local-orb")

ior = open(posix.environ['HOME'] + "/.xine.ior").readline()
xine = orb.string_to_object(ior)

try:
	xine.stop()
	xine.play(sys.argv[1], 0)
except CORBA.error, data:
	print "CORBA error:", data
