"""Remote-control interfaces to common browsers."""

import os
import sys

class Error(Exception):
    pass

_browsers = {}		# Dictionary of available browser controllers
_tryorder = []		# Preference order of available browsers

def register(name, klass, instance=None):
    """Register a browser connector and, optionally, connection."""
    _browsers[name.lower()] = [klass, instance]

def get(using=None):
    """Return a browser launcher instance appropriate for the environment."""
    if using:
        alternatives = [using]
    else:
        alternatives = _tryorder
    for browser in alternatives:
        if browser.find('%s') > -1:
            # User gave us a command line, don't mess with it.
            return browser
        else:
            # User gave us a browser name. 
            command = _browsers[browser.lower()]
            if command[1] is None:
                return command[0]()
            else:
                return command[1]
    raise Error("could not locate runnable browser")

# Please note: the following definition hides a builtin function.

def open(url, new=0, autoraise=1):
    get().open(url, new, autoraise)

def open_new(url):	# Marked deprecated.  May be removed in 2.1.
    get().open(url, 1)

#
# Everything after this point initializes _browsers and _tryorder,
# then disappears.  Some class definitions and instances remain
# live through these globals, but only the minimum set needed to
# support the user's platform.
#

# 
# Platform support for Unix
#

# This is the right test because all these Unix browsers require either
# a console terminal of an X display to run.  Note that we cannot split
# the TERM and DISPLAY cases, because we might be running Python from inside
# an xterm.
if os.environ.get("TERM") or os.environ.get("DISPLAY"):
    PROCESS_CREATION_DELAY = 4
    global tryorder
    _tryorder = ("mozilla","netscape","kfm","grail","links","lynx","w3m")

    def _iscommand(cmd):
        """Return true if cmd can be found on the executable search path."""
        path = os.environ.get("PATH")
        if not path:
            return 0
        for d in path.split(os.pathsep):
            exe = os.path.join(d, cmd)
            if os.path.isfile(exe):
                return 1
        return 0

    class GenericBrowser:
        def __init__(self, cmd):
            self.command = cmd

        def open(self, url, new=0, autoraise=1):
            os.system(self.command % url)

        def open_new(self, url):	# Deprecated.  May be removed in 2.1.
            self.open(url)

    # Easy cases first -- register console browsers if we have them.
    if os.environ.get("TERM"):
        # The Links browser <http://artax.karlin.mff.cuni.cz/~mikulas/links/>
        if _iscommand("links"):
            register("links", None, GenericBrowser("links %s"))
        # The Lynx browser <http://lynx.browser.org/>
        if _iscommand("lynx"):
            register("lynx", None, GenericBrowser("lynx %s"))
        # The w3m browser <http://ei5nazha.yz.yamagata-u.ac.jp/~aito/w3m/eng/>
        if _iscommand("w3m"):
            register("w3m", None, GenericBrowser("w3m %s"))

    # X browsers have mre in the way of options
    if os.environ.get("DISPLAY"):
        # First, the Netscape series
        if _iscommand("netscape") or _iscommand("mozilla"):
            class Netscape:
                "Launcher class for Netscape browsers."
                def __init__(self, name):
                    self.name = name

                def _remote(self, action, autoraise):
                    raise_opt = ("-noraise", "-raise")[autoraise]
                    cmd = "%s %s -remote '%s' >/dev/null 2>&1" % (self.name, raise_opt, action)
                    rc = os.system(cmd)
                    if rc:
                        import time
                        os.system("%s -no-about-splash &" % self.name)
                        time.sleep(PROCESS_CREATION_DELAY)
                        rc = os.system(cmd)
                    return not rc

                def open(self, url, new=0, autoraise=1):
                    if new:
                        self._remote("openURL(%s, new-window)"%url, autoraise)
                    else:
                        self._remote("openURL(%s)" % url, autoraise)

                # Deprecated.  May be removed in 2.1.
                def open_new(self, url):
                    self.open(url, 1)

            if _iscommand("mozilla"):
                register("mozilla", None, Netscape("mozilla"))
            if _iscommand("netscape"):
                register("netscape", None, Netscape("netscape"))

        # Next, Mosaic -- old but still in use.
        if _iscommand("mosaic"):
            register("mosaic", None, GenericBrowser("mosaic %s >/dev/null &"))

        # Konqueror/kfm, the KDE browser.
        if _iscommand("kfm"):
            class Konqueror:
                """Controller for the KDE File Manager (kfm, or Konqueror).

                See http://developer.kde.org/documentation/other/kfmclient.html
                for more information on the Konqueror remote-control interface.

                """
                def _remote(self, action):
                    cmd = "kfmclient %s >/dev/null 2>&1" % action
                    rc = os.system(cmd)
                    if rc:
                        import time
                        os.system("kfm -d &")
                        time.sleep(PROCESS_CREATION_DELAY)
                        rc = os.system(cmd)
                    return not rc

                def open(self, url, new=1, autoraise=1):
                    # XXX Currently I know no way to prevent KFM from opening a new win.
                    self._remote("openURL %s" % url)

                # Deprecated.  May be removed in 2.1.
                open_new = open

            register("kfm", Konqueror, None)

        # Grail, the Python browser.
        if _iscommand("grail"):
            class Grail:
                # There should be a way to maintain a connection to
                # Grail, but the Grail remote control protocol doesn't
                # really allow that at this point.  It probably neverwill!
                def _find_grail_rc(self):
                    import glob
                    import pwd
                    import socket
                    import tempfile
                    tempdir = os.path.join(tempfile.gettempdir(), ".grail-unix")
                    user = pwd.getpwuid(_os.getuid())[0]
                    filename = os.path.join(tempdir, user + "-*")
                    maybes = glob.glob(filename)
                    if not maybes:
                        return None
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    for fn in maybes:
                        # need to PING each one until we find one that's live
                        try:
                            s.connect(fn)
                        except socket.error:
                            # no good; attempt to clean it out, but don't fail:
                            try:
                                os.unlink(fn)
                            except IOError:
                                pass
                        else:
                            return s

                def _remote(self, action):
                    s = self._find_grail_rc()
                    if not s:
                        return 0
                    s.send(action)
                    s.close()
                    return 1

                def open(self, url, new=0, autoraise=1):
                    if new:
                        self._remote("LOADNEW " + url)
                    else:
                        self._remote("LOAD " + url)

                # Deprecated.  May be removed in 2.1.
                def open_new(self, url):
                    self.open(url, 1)

            register("grail", Grail, None)

#
# Platform support for Windows
#

if sys.platform[:3] == "win":
    global _tryorder
    _tryorder = ("netscape", "windows-default")

    class WindowsDefault:
        def open(self, url, new=0, autoraise=1):
            os.startfile(url)

        def open_new(self, url):        # Deprecated.  May be removed in 2.1.
            self.open(url)

    register("windows-default", WindowsDefault)

#
# Platform support for MacOS
#

try:
    import ic
except ImportError:
    pass
else:
    class InternetConfig:
        def open(self, url, new=0, autoraise=1):
            ic.launchurl(url)

        def open_new(self, url):        # Deprecated.  May be removed in 2.1.
            self.open(url)

    # internet-config is the only supported controller on MacOS,
    # so don't mess with the default!
    _tryorder = ("internet-config")
    register("internet-config", InternetConfig)

# OK, now that we know what the default preference orders for each
# platform are, allow user to override them with the BROWSER variable.
#
if os.environ.has_key("BROWSER"):
    # It's the user's responsibility to register handlers for any unknown
    # browser referenced by this value, before calling open().
    _tryorder = os.environ["BROWSER"].split(":")
else:
    # Optimization: filter out alternatives that aren't available, so we can
    # avoid has_key() tests at runtime.  (This may also allow some unused
    # classes and class-instance storage to be garbage-collected.)
    _tryorder = filter(lambda x: _browsers.has_key(x.lower()) or x.find("%s")>-1,\
                       _tryorder)

# end
