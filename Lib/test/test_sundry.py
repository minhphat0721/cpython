"""Do a minimal test of all the modules that aren't otherwise tested."""

from test_support import verbose

import BaseHTTPServer
import CGIHTTPServer
import Queue
import SimpleHTTPServer
import SocketServer
import aifc
import anydbm
#import asynchat
import asyncore
import audiodev
import bdb
import cmd
import code
import codeop
import colorsys
import commands
import compileall
try:
    import curses   # not available on Windows
except ImportError:
    if verbose:
        print "skipping curses"
import dbhash
import dircache
import dis
import distutils
import doctest
import dumbdbm
import encodings
import filecmp
import fileinput
import fnmatch
import formatter
import fpformat
import ftplib
import getpass
import glob
import gopherlib
import htmlentitydefs
import htmllib
import httplib
import imaplib
import imghdr
import imputil
import keyword
#import knee
import macpath
import macurl2path
import mailcap
import mhlib
import mimetypes
import mimify
import multifile
import mutex
import netrc
import nntplib
import nturl2path
import pdb
import pipes
#import poplib
import posixfile
import pre
import profile
import pstats
import py_compile
import pyclbr
#import reconvert
import repr
try:
    import rlcompleter   # not available on Windows
except ImportError:
    if verbose:
        print "skipping rlcompleter"
import robotparser
import sched
import sgmllib
import shelve
import shlex
import shutil
import smtplib
import sndhdr
import statcache
import statvfs
import stringold
import sunau
import sunaudio
import symbol
import tabnanny
import telnetlib
import test
import toaiff
#import tzparse
import urllib2
# Can't test the "user" module -- if the user has a ~/.pythonrc.py, it
# can screw up all sorts of things (esp. if it prints!).
#import user
import uu
import webbrowser
import whichdb
import xdrlib
import xml
