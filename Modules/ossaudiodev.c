/*
 * ossaudiodev -- Python interface to the OSS (Open Sound System) API.
 *                This is the standard audio API for Linux and some
 *                flavours of BSD [XXX which ones?]; it is also available
 *                for a wide range of commercial Unices.
 *
 * Originally written by Peter Bosch, March 2000, as linuxaudiodev.
 *
 * Renamed to ossaudiodev and rearranged/revised/hacked up
 * by Greg Ward <gward@python.net>, November 2002.
 * Mixer interface by Nicholas FitzRoy-Dale <wzdd@lardcave.net>, Dec 2002.
 *              
 * (c) 2000 Peter Bosch.  All Rights Reserved.
 * (c) 2002 Gregory P. Ward.  All Rights Reserved.
 * (c) 2002 Python Software Foundation.  All Rights Reserved.
 *
 * XXX need a license statement
 *
 * $Id$
 */

#include "Python.h"
#include "structmember.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#define O_RDONLY 00
#define O_WRONLY 01
#endif


#include <sys/ioctl.h>
#if defined(linux)
#include <linux/soundcard.h>

typedef unsigned long uint32_t;

#elif defined(__FreeBSD__)
#include <machine/soundcard.h>

#ifndef SNDCTL_DSP_CHANNELS
#define SNDCTL_DSP_CHANNELS SOUND_PCM_WRITE_CHANNELS
#endif

#endif

typedef struct {
    PyObject_HEAD;
    int      fd;                      /* The open file */
    int      mode;                    /* file mode */
    int      icount;                  /* Input count */
    int      ocount;                  /* Output count */
    uint32_t afmts;                   /* Audio formats supported by hardware */
} oss_t;

typedef struct {
    PyObject_HEAD;
    int      fd;                      /* The open mixer device */
} oss_mixer_t;

/* XXX several format defined in soundcard.h are not supported,
   including _NE (native endian) options and S32 options
*/

static struct {
    int         a_bps;
    uint32_t    a_fmt;
    char       *a_name;
} audio_types[] = {
    {  8,       AFMT_MU_LAW, "logarithmic mu-law 8-bit audio" },
    {  8,       AFMT_A_LAW,  "logarithmic A-law 8-bit audio" },
    {  8,       AFMT_U8,     "linear unsigned 8-bit audio" },
    {  8,       AFMT_S8,     "linear signed 8-bit audio" },
    { 16,       AFMT_U16_BE, "linear unsigned 16-bit big-endian audio" },
    { 16,       AFMT_U16_LE, "linear unsigned 16-bit little-endian audio" },
    { 16,       AFMT_S16_BE, "linear signed 16-bit big-endian audio" },
    { 16,       AFMT_S16_LE, "linear signed 16-bit little-endian audio" },
    { 16,       AFMT_S16_NE, "linear signed 16-bit native-endian audio" },
};

static int n_audio_types = sizeof(audio_types) / sizeof(audio_types[0]);

static PyTypeObject OSSType;
static PyTypeObject OSSMixerType;

static PyObject *OSSAudioError;

static oss_t *
newossobject(PyObject *arg)
{
    oss_t *xp;
    int fd, afmts, imode;
    char *basedev = NULL;
    char *mode = NULL;

    /* Two ways to call open():
         open(device, mode) (for consistency with builtin open())
         open(mode)         (for backwards compatibility)
       because the *first* argument is optional, parsing args is
       a wee bit tricky. */
    if (!PyArg_ParseTuple(arg, "s|s:open", &basedev, &mode))
       return NULL;
    if (mode == NULL) {                 /* only one arg supplied */
       mode = basedev;
       basedev = NULL;
    }

    if (strcmp(mode, "r") == 0)
        imode = O_RDONLY;
    else if (strcmp(mode, "w") == 0)
        imode = O_WRONLY;
    else if (strcmp(mode, "rw") == 0)
        imode = O_RDWR;
    else {
        PyErr_SetString(OSSAudioError, "mode must be 'r', 'w', or 'rw'");
        return NULL;
    }

    /* Open the correct device: either the 'device' argument,
       or the AUDIODEV environment variable, or "/dev/dsp". */
    if (basedev == NULL) {              /* called with one arg */
       basedev = getenv("AUDIODEV");
       if (basedev == NULL)             /* $AUDIODEV not set */
          basedev = "/dev/dsp";
    }

    if ((fd = open(basedev, imode)) == -1) {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, basedev);
        return NULL;
    }
    if (ioctl(fd, SNDCTL_DSP_GETFMTS, &afmts) == -1) {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, basedev);
        return NULL;
    }
    /* Create and initialize the object */
    if ((xp = PyObject_New(oss_t, &OSSType)) == NULL) {
        close(fd);
        return NULL;
    }
    xp->fd = fd;
    xp->mode = imode;
    xp->icount = xp->ocount = 0;
    xp->afmts  = afmts;
    return xp;
}

static void
oss_dealloc(oss_t *xp)
{
    /* if already closed, don't reclose it */
    if (xp->fd != -1)
        close(xp->fd);
    PyObject_Del(xp);
}

static oss_mixer_t *
newossmixerobject(PyObject *arg)
{
    char *basedev = NULL, *mode = NULL;
    int fd, imode;
    oss_mixer_t *xp;
    
    if (!PyArg_ParseTuple(arg, "|ss", &basedev, &mode)) {
        return NULL;
    }
    
    if (basedev == NULL) {
        basedev = getenv("MIXERDEV");
        if (basedev == NULL)            /* MIXERDEV not set */
            basedev = "/dev/mixer";
    }

    if (mode == NULL || strcmp(mode, "r") == 0)
        imode = O_RDONLY;
    else if (strcmp(mode, "w") == 0)
        imode = O_WRONLY;
    else if (strcmp(mode, "rw") == 0)
        imode = O_RDWR;
    else {
        PyErr_SetString(OSSAudioError, "mode must be 'r', 'w', or 'rw'");
        return NULL;
    }

    if ((fd = open(basedev, imode)) == -1) {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, basedev);
        return NULL;
    }
    
    if ((xp = PyObject_New(oss_mixer_t, &OSSMixerType)) == NULL) {
        close(fd);
        return NULL;
    }
    
    xp->fd = fd;
    
    return xp;
}

static void
oss_mixer_dealloc(oss_mixer_t *xp)
{
    /* if already closed, don't reclose it */
    if (xp->fd != -1)
        close(xp->fd);
    PyObject_Del(xp);
}


/* Methods to wrap the OSS ioctls.  The calling convention is pretty
   simple:
     nonblock()        -> ioctl(fd, SNDCTL_DSP_NONBLOCK)
     fmt = setfmt(fmt) -> ioctl(fd, SNDCTL_DSP_SETFMT, &fmt)
     etc.
*/


/* _do_ioctl_1() is a private helper function used for the OSS ioctls --
   SNDCTL_DSP_{SETFMT,CHANNELS,SPEED} -- that that are called from C
   like this:
     ioctl(fd, SNDCTL_DSP_cmd, &arg)

   where arg is the value to set, and on return the driver sets arg to
   the value that was actually set.  Mapping this to Python is obvious:
     arg = dsp.xxx(arg)
*/
static PyObject *
_do_ioctl_1(int fd, PyObject *args, char *fname, int cmd)
{
    char argfmt[33] = "i:";
    int arg;

    assert(strlen(fname) <= 30);
    strcat(argfmt, fname);
    if (!PyArg_ParseTuple(args, argfmt, &arg))
        return NULL;

    if (ioctl(fd, cmd, &arg) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    return PyInt_FromLong(arg);
}

/* _do_ioctl_1_internal() is a wrapper for ioctls that take no inputs
   but return an output -- ie. we need to pass a pointer to a local C
   variable so the driver can write its output there, but from Python
   all we see is the return value.  For example,
   SOUND_MIXER_READ_DEVMASK returns a bitmask of available mixer
   devices, but does not use the value of the parameter passed-in in any
   way.
*/

static PyObject *
_do_ioctl_1_internal(int fd, PyObject *args, char *fname, int cmd)
{
    char argfmt[32] = ":";
    int arg = 0;

    assert(strlen(fname) <= 30);
    strcat(argfmt, fname);
    if (!PyArg_ParseTuple(args, argfmt, &arg))
        return NULL;

    if (ioctl(fd, cmd, &arg) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    return PyInt_FromLong(arg);
}



/* _do_ioctl_0() is a private helper for the no-argument ioctls:
   SNDCTL_DSP_{SYNC,RESET,POST}. */
static PyObject *
_do_ioctl_0(int fd, PyObject *args, char *fname, int cmd)
{
    char argfmt[32] = ":";

    assert(strlen(fname) <= 30);
    strcat(argfmt, fname);
    if (!PyArg_ParseTuple(args, argfmt))
        return NULL;

    if (ioctl(fd, cmd, 0) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
oss_nonblock(oss_t *self, PyObject *args)
{
    /* Hmmm: it doesn't appear to be possible to return to blocking
       mode once we're in non-blocking mode! */
    if (!PyArg_ParseTuple(args, ":nonblock"))
        return NULL;
    if (ioctl(self->fd, SNDCTL_DSP_NONBLOCK, NULL) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
oss_setfmt(oss_t *self, PyObject *args)
{
    return _do_ioctl_1(self->fd, args, "setfmt", SNDCTL_DSP_SETFMT);
}

static PyObject *
oss_getfmts(oss_t *self, PyObject *args)
{
    int mask;
    if (!PyArg_ParseTuple(args, ":getfmts"))
        return NULL;
    if (ioctl(self->fd, SNDCTL_DSP_GETFMTS, &mask) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    return PyInt_FromLong(mask);
}

static PyObject *
oss_channels(oss_t *self, PyObject *args)
{
    return _do_ioctl_1(self->fd, args, "channels", SNDCTL_DSP_CHANNELS);
}

static PyObject *
oss_speed(oss_t *self, PyObject *args)
{
    return _do_ioctl_1(self->fd, args, "speed", SNDCTL_DSP_SPEED);
}

static PyObject *
oss_sync(oss_t *self, PyObject *args)
{
    return _do_ioctl_0(self->fd, args, "sync", SNDCTL_DSP_SYNC);
}
    
static PyObject *
oss_reset(oss_t *self, PyObject *args)
{
    return _do_ioctl_0(self->fd, args, "reset", SNDCTL_DSP_RESET);
}
    
static PyObject *
oss_post(oss_t *self, PyObject *args)
{
    return _do_ioctl_0(self->fd, args, "post", SNDCTL_DSP_POST);
}


/* Regular file methods: read(), write(), close(), etc. as well
   as one convenience method, writeall(). */

static PyObject *
oss_read(oss_t *self, PyObject *args)
{
    int size, count;
    char *cp;
    PyObject *rv;
        
    if (!PyArg_ParseTuple(args, "i:read", &size))
        return NULL;
    rv = PyString_FromStringAndSize(NULL, size);
    if (rv == NULL)
        return NULL;
    cp = PyString_AS_STRING(rv);
    if ((count = read(self->fd, cp, size)) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        Py_DECREF(rv);
        return NULL;
    }
    self->icount += count;
    _PyString_Resize(&rv, count);
    return rv;
}

static PyObject *
oss_write(oss_t *self, PyObject *args)
{
    char *cp;
    int rv, size;

    if (!PyArg_ParseTuple(args, "s#:write", &cp, &size)) {
        return NULL;
    }
    if ((rv = write(self->fd, cp, size)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    } else {
        self->ocount += rv;
    }
    return PyInt_FromLong(rv);
}

static PyObject *
oss_writeall(oss_t *self, PyObject *args)
{
    char *cp;
    int rv, size;
    fd_set write_set_fds;
    int select_rv;
    
    /* NB. writeall() is only useful in non-blocking mode: according to
       Guenter Geiger <geiger@xdv.org> on the linux-audio-dev list
       (http://eca.cx/lad/2002/11/0380.html), OSS guarantees that
       write() in blocking mode consumes the whole buffer.  In blocking
       mode, the behaviour of write() and writeall() from Python is
       indistinguishable. */

    if (!PyArg_ParseTuple(args, "s#:write", &cp, &size)) 
        return NULL;

    /* use select to wait for audio device to be available */
    FD_ZERO(&write_set_fds);
    FD_SET(self->fd, &write_set_fds);

    while (size > 0) {
        select_rv = select(self->fd+1, NULL, &write_set_fds, NULL, NULL);
        assert(select_rv != 0);         /* no timeout, can't expire */
        if (select_rv == -1)
            return PyErr_SetFromErrno(PyExc_IOError);

        rv = write(self->fd, cp, size);
        if (rv == -1) {
            if (errno == EAGAIN) {      /* buffer is full, try again */
                errno = 0;
                continue;
            } else                      /* it's a real error */
                return PyErr_SetFromErrno(PyExc_IOError);
        } else {                        /* wrote rv bytes */
            self->ocount += rv;
            size -= rv;
            cp += rv;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
oss_close(oss_t *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    if (self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
oss_fileno(oss_t *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":fileno")) 
        return NULL;
    return PyInt_FromLong(self->fd);
}


/* Convenience methods: these generally wrap a couple of ioctls into one
   common task. */

static PyObject *
oss_setparameters(oss_t *self, PyObject *args)
{
    int rate, ssize, nchannels, n, fmt, emulate=0;

    if (!PyArg_ParseTuple(args, "iiii|i:setparameters",
                          &rate, &ssize, &nchannels, &fmt, &emulate))
        return NULL;
  
    if (rate < 0) {
        PyErr_Format(PyExc_ValueError, "expected rate >= 0, not %d",
                     rate); 
        return NULL;
    }
    if (ssize < 0) {
        PyErr_Format(PyExc_ValueError, "expected sample size >= 0, not %d",
                     ssize);
        return NULL;
    }
    if (nchannels != 1 && nchannels != 2) {
        PyErr_Format(PyExc_ValueError, "nchannels must be 1 or 2, not %d",
                     nchannels);
        return NULL;
    }

    for (n = 0; n < n_audio_types; n++)
        if (fmt == audio_types[n].a_fmt)
            break;
    if (n == n_audio_types) {
        PyErr_Format(PyExc_ValueError, "unknown audio encoding: %d", fmt);
        return NULL;
    }
    if (audio_types[n].a_bps != ssize) {
        PyErr_Format(PyExc_ValueError, 
                     "for %s, expected sample size %d, not %d",
                     audio_types[n].a_name, audio_types[n].a_bps, ssize);
        return NULL;
    }

    if (emulate == 0) {
        if ((self->afmts & audio_types[n].a_fmt) == 0) {
            PyErr_Format(PyExc_ValueError, 
                         "%s format not supported by device",
                         audio_types[n].a_name);
            return NULL;
        }
    }
    if (ioctl(self->fd, SNDCTL_DSP_SETFMT, 
              &audio_types[n].a_fmt) == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    if (ioctl(self->fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    if (ioctl(self->fd, SNDCTL_DSP_SPEED, &rate) == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static int
_ssize(oss_t *self, int *nchannels, int *ssize)
{
    int fmt;

    fmt = 0;
    if (ioctl(self->fd, SNDCTL_DSP_SETFMT, &fmt) < 0) 
        return -errno;

    switch (fmt) {
    case AFMT_MU_LAW:
    case AFMT_A_LAW:
    case AFMT_U8:
    case AFMT_S8:
        *ssize = sizeof(char);
        break;
    case AFMT_S16_LE:
    case AFMT_S16_BE:
    case AFMT_U16_LE:
    case AFMT_U16_BE:
        *ssize = sizeof(short);
        break;
    case AFMT_MPEG:
    case AFMT_IMA_ADPCM:
    default:
        return -EOPNOTSUPP;
    }
    *nchannels = 0;
    if (ioctl(self->fd, SNDCTL_DSP_CHANNELS, nchannels) < 0)
        return -errno;
    return 0;
}


/* bufsize returns the size of the hardware audio buffer in number 
   of samples */
static PyObject *
oss_bufsize(oss_t *self, PyObject *args)
{
    audio_buf_info ai;
    int nchannels, ssize;

    if (!PyArg_ParseTuple(args, ":bufsize")) return NULL;

    if (_ssize(self, &nchannels, &ssize) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    return PyInt_FromLong((ai.fragstotal * ai.fragsize) / (nchannels * ssize));
}

/* obufcount returns the number of samples that are available in the 
   hardware for playing */
static PyObject *
oss_obufcount(oss_t *self, PyObject *args)
{
    audio_buf_info ai;
    int nchannels, ssize;

    if (!PyArg_ParseTuple(args, ":obufcount"))
        return NULL;

    if (_ssize(self, &nchannels, &ssize) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    return PyInt_FromLong((ai.fragstotal * ai.fragsize - ai.bytes) / 
                          (ssize * nchannels));
}

/* obufcount returns the number of samples that can be played without
   blocking */
static PyObject *
oss_obuffree(oss_t *self, PyObject *args)
{
    audio_buf_info ai;
    int nchannels, ssize;

    if (!PyArg_ParseTuple(args, ":obuffree"))
        return NULL;

    if (_ssize(self, &nchannels, &ssize) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    return PyInt_FromLong(ai.bytes / (ssize * nchannels));
}

static PyObject *
oss_getptr(oss_t *self, PyObject *args)
{
    count_info info;
    int req;

    if (!PyArg_ParseTuple(args, ":getptr"))
        return NULL;
    
    if (self->mode == O_RDONLY)
        req = SNDCTL_DSP_GETIPTR;
    else
        req = SNDCTL_DSP_GETOPTR;
    if (ioctl(self->fd, req, &info) == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    return Py_BuildValue("iii", info.bytes, info.blocks, info.ptr);
}

/* Mixer methods */
static PyObject *
oss_mixer_close(oss_mixer_t *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    if (self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
oss_mixer_fileno(oss_mixer_t *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":fileno")) 
        return NULL;
    return PyInt_FromLong(self->fd);
}

/* Simple mixer interface methods */

static PyObject *
oss_mixer_devices(oss_mixer_t *self, PyObject *args)
{
    return _do_ioctl_1_internal(self->fd, args, "devices",
        SOUND_MIXER_READ_DEVMASK);
}

static PyObject *
oss_mixer_stereodevices(oss_mixer_t *self, PyObject *args)
{
    return _do_ioctl_1_internal(self->fd, args, "stereodevices",
        SOUND_MIXER_READ_STEREODEVS);
}

static PyObject *
oss_mixer_recdevices(oss_mixer_t *self, PyObject *args)
{
    return _do_ioctl_1_internal(self->fd, args, "recdevices",
        SOUND_MIXER_READ_RECMASK);
}

static PyObject *
oss_mixer_get(oss_mixer_t *self, PyObject *args)
{
    int channel, volume;
    
    /* Can't use _do_ioctl_1 because of encoded arg thingy. */
    if (!PyArg_ParseTuple(args, "i:get", &channel))
        return NULL;
    
    if (channel < 0 || channel > SOUND_MIXER_NRDEVICES) {
        PyErr_SetString(OSSAudioError, "Invalid mixer channel specified.");
        return NULL;
    }
    
    if (ioctl(self->fd, MIXER_READ(channel), &volume) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
    
    return Py_BuildValue("(ii)", volume & 0xff, (volume & 0xff00) >> 8);
}

static PyObject *
oss_mixer_set(oss_mixer_t *self, PyObject *args)
{
    int channel, volume, leftVol, rightVol;
    
    /* Can't use _do_ioctl_1 because of encoded arg thingy. */
    if (!PyArg_ParseTuple(args, "i(ii):set", &channel, &leftVol, &rightVol))
        return NULL;
            
    if (channel < 0 || channel > SOUND_MIXER_NRDEVICES) {
        PyErr_SetString(OSSAudioError, "Invalid mixer channel specified.");
        return NULL;
    }
    
    if (leftVol < 0 || rightVol < 0 || leftVol > 100 || rightVol > 100) {
        PyErr_SetString(OSSAudioError, "Volumes must be between 0 and 100.");
        return NULL;
    }

    volume = (rightVol << 8) | leftVol;
    
    if (ioctl(self->fd, MIXER_WRITE(channel), &volume) == -1)
        return PyErr_SetFromErrno(PyExc_IOError);
   
    return Py_BuildValue("(ii)", volume & 0xff, (volume & 0xff00) >> 8);
}

static PyObject *
oss_mixer_get_recsrc(oss_mixer_t *self, PyObject *args)
{
    return _do_ioctl_1_internal(self->fd, args, "get_recsrc",
        SOUND_MIXER_READ_RECSRC);
}

static PyObject *
oss_mixer_set_recsrc(oss_mixer_t *self, PyObject *args)
{
    return _do_ioctl_1(self->fd, args, "set_recsrc",
        SOUND_MIXER_WRITE_RECSRC);
}


static PyMethodDef oss_methods[] = {
    /* Regular file methods */
    { "read",           (PyCFunction)oss_read, METH_VARARGS },
    { "write",          (PyCFunction)oss_write, METH_VARARGS },
    { "writeall",       (PyCFunction)oss_writeall, METH_VARARGS },
    { "close",          (PyCFunction)oss_close, METH_VARARGS },
    { "fileno",         (PyCFunction)oss_fileno, METH_VARARGS },

    /* Simple ioctl wrappers */
    { "nonblock",       (PyCFunction)oss_nonblock, METH_VARARGS },
    { "setfmt",         (PyCFunction)oss_setfmt, METH_VARARGS },
    { "getfmts",        (PyCFunction)oss_getfmts, METH_VARARGS },
    { "channels",       (PyCFunction)oss_channels, METH_VARARGS },
    { "speed",          (PyCFunction)oss_speed, METH_VARARGS },
    { "sync",           (PyCFunction)oss_sync, METH_VARARGS },
    { "reset",          (PyCFunction)oss_reset, METH_VARARGS },
    { "post",           (PyCFunction)oss_post, METH_VARARGS },

    /* Convenience methods -- wrap a couple of ioctls together */
    { "setparameters",  (PyCFunction)oss_setparameters, METH_VARARGS },
    { "bufsize",        (PyCFunction)oss_bufsize, METH_VARARGS },
    { "obufcount",      (PyCFunction)oss_obufcount, METH_VARARGS },
    { "obuffree",       (PyCFunction)oss_obuffree, METH_VARARGS },
    { "getptr",         (PyCFunction)oss_getptr, METH_VARARGS },

    /* Aliases for backwards compatibility */
    { "flush",          (PyCFunction)oss_sync, METH_VARARGS },

    { NULL,             NULL}           /* sentinel */
};

static PyMethodDef oss_mixer_methods[] = {
    /* Regular file method - OSS mixers are ioctl-only interface */
    { "close",          (PyCFunction)oss_mixer_close, METH_VARARGS },   
    { "fileno",         (PyCFunction)oss_mixer_fileno, METH_VARARGS },

    /* Simple ioctl wrappers */
    { "devices",        (PyCFunction)oss_mixer_devices, METH_VARARGS }, 
    { "stereodevices",  (PyCFunction)oss_mixer_stereodevices, METH_VARARGS},
    { "recdevices",     (PyCFunction)oss_mixer_recdevices, METH_VARARGS},   
    { "get",            (PyCFunction)oss_mixer_get, METH_VARARGS },
    { "set",            (PyCFunction)oss_mixer_set, METH_VARARGS },
    { "get_recsrc",     (PyCFunction)oss_mixer_get_recsrc, METH_VARARGS },
    { "set_recsrc",     (PyCFunction)oss_mixer_set_recsrc, METH_VARARGS },
    
    { NULL,             NULL}
};

static PyObject *
oss_getattr(oss_t *xp, char *name)
{
    return Py_FindMethod(oss_methods, (PyObject *)xp, name);
}

static PyObject *
oss_mixer_getattr(oss_mixer_t *xp, char *name)
{
    return Py_FindMethod(oss_mixer_methods, (PyObject *)xp, name);
}

static PyTypeObject OSSType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                          /*ob_size*/
    "ossaudiodev.oss_audio_device", /*tp_name*/
    sizeof(oss_t),              /*tp_size*/
    0,                          /*tp_itemsize*/
    /* methods */
    (destructor)oss_dealloc,    /*tp_dealloc*/
    0,                          /*tp_print*/
    (getattrfunc)oss_getattr,   /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
};

static PyTypeObject OSSMixerType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                              /*ob_size*/
    "ossaudiodev.oss_mixer_device", /*tp_name*/
    sizeof(oss_mixer_t),            /*tp_size*/
    0,                              /*tp_itemsize*/
    /* methods */
    (destructor)oss_mixer_dealloc,  /*tp_dealloc*/
    0,                              /*tp_print*/
    (getattrfunc)oss_mixer_getattr, /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
};


static PyObject *
ossopen(PyObject *self, PyObject *args)
{
    return (PyObject *)newossobject(args);
}

static PyObject *
ossopenmixer(PyObject *self, PyObject *args)
{
    return (PyObject *)newossmixerobject(args);
}

static PyMethodDef ossaudiodev_methods[] = {
    { "open", ossopen, METH_VARARGS },
    { "openmixer", ossopenmixer, METH_VARARGS },
    { 0, 0 },
};


#define _EXPORT_INT(mod, name) \
  if (PyModule_AddIntConstant(mod, #name, (long) (name)) == -1) return;

void
initossaudiodev(void)
{
    PyObject *m;
  
    m = Py_InitModule("ossaudiodev", ossaudiodev_methods);

    OSSAudioError = PyErr_NewException("ossaudiodev.error", NULL, NULL);
    if (OSSAudioError)
        PyModule_AddObject(m, "error", OSSAudioError);

    /* Expose the audio format numbers -- essential! */
    _EXPORT_INT(m, AFMT_QUERY);
    _EXPORT_INT(m, AFMT_MU_LAW);
    _EXPORT_INT(m, AFMT_A_LAW);
    _EXPORT_INT(m, AFMT_IMA_ADPCM);
    _EXPORT_INT(m, AFMT_U8);
    _EXPORT_INT(m, AFMT_S16_LE);
    _EXPORT_INT(m, AFMT_S16_BE);
    _EXPORT_INT(m, AFMT_S8);
    _EXPORT_INT(m, AFMT_U16_LE);
    _EXPORT_INT(m, AFMT_U16_BE);
    _EXPORT_INT(m, AFMT_MPEG);
    _EXPORT_INT(m, AFMT_AC3);
    _EXPORT_INT(m, AFMT_S16_NE);
        
    /* Expose the sound mixer device numbers. */
    _EXPORT_INT(m, SOUND_MIXER_NRDEVICES);
    _EXPORT_INT(m, SOUND_MIXER_VOLUME);
    _EXPORT_INT(m, SOUND_MIXER_BASS);
    _EXPORT_INT(m, SOUND_MIXER_TREBLE);
    _EXPORT_INT(m, SOUND_MIXER_SYNTH);
    _EXPORT_INT(m, SOUND_MIXER_PCM);
    _EXPORT_INT(m, SOUND_MIXER_SPEAKER);
    _EXPORT_INT(m, SOUND_MIXER_LINE);
    _EXPORT_INT(m, SOUND_MIXER_MIC);
    _EXPORT_INT(m, SOUND_MIXER_CD);
    _EXPORT_INT(m, SOUND_MIXER_IMIX);
    _EXPORT_INT(m, SOUND_MIXER_ALTPCM);
    _EXPORT_INT(m, SOUND_MIXER_RECLEV);
    _EXPORT_INT(m, SOUND_MIXER_IGAIN);
    _EXPORT_INT(m, SOUND_MIXER_OGAIN);
    _EXPORT_INT(m, SOUND_MIXER_LINE1);
    _EXPORT_INT(m, SOUND_MIXER_LINE2);
    _EXPORT_INT(m, SOUND_MIXER_LINE3);
    _EXPORT_INT(m, SOUND_MIXER_DIGITAL1);
    _EXPORT_INT(m, SOUND_MIXER_DIGITAL2);
    _EXPORT_INT(m, SOUND_MIXER_DIGITAL3);
    _EXPORT_INT(m, SOUND_MIXER_PHONEIN);
    _EXPORT_INT(m, SOUND_MIXER_PHONEOUT);
    _EXPORT_INT(m, SOUND_MIXER_VIDEO);
    _EXPORT_INT(m, SOUND_MIXER_RADIO);
    _EXPORT_INT(m, SOUND_MIXER_MONITOR);

    /* Expose all the ioctl numbers for masochists who like to do this
       stuff directly. */
    _EXPORT_INT(m, SNDCTL_COPR_HALT);
    _EXPORT_INT(m, SNDCTL_COPR_LOAD);
    _EXPORT_INT(m, SNDCTL_COPR_RCODE);
    _EXPORT_INT(m, SNDCTL_COPR_RCVMSG);
    _EXPORT_INT(m, SNDCTL_COPR_RDATA);
    _EXPORT_INT(m, SNDCTL_COPR_RESET);
    _EXPORT_INT(m, SNDCTL_COPR_RUN);
    _EXPORT_INT(m, SNDCTL_COPR_SENDMSG);
    _EXPORT_INT(m, SNDCTL_COPR_WCODE);
    _EXPORT_INT(m, SNDCTL_COPR_WDATA);
    _EXPORT_INT(m, SNDCTL_DSP_BIND_CHANNEL);
    _EXPORT_INT(m, SNDCTL_DSP_CHANNELS);
    _EXPORT_INT(m, SNDCTL_DSP_GETBLKSIZE);
    _EXPORT_INT(m, SNDCTL_DSP_GETCAPS);
    _EXPORT_INT(m, SNDCTL_DSP_GETCHANNELMASK);
    _EXPORT_INT(m, SNDCTL_DSP_GETFMTS);
    _EXPORT_INT(m, SNDCTL_DSP_GETIPTR);
    _EXPORT_INT(m, SNDCTL_DSP_GETISPACE);
    _EXPORT_INT(m, SNDCTL_DSP_GETODELAY);
    _EXPORT_INT(m, SNDCTL_DSP_GETOPTR);
    _EXPORT_INT(m, SNDCTL_DSP_GETOSPACE);
    _EXPORT_INT(m, SNDCTL_DSP_GETSPDIF);
    _EXPORT_INT(m, SNDCTL_DSP_GETTRIGGER);
    _EXPORT_INT(m, SNDCTL_DSP_MAPINBUF);
    _EXPORT_INT(m, SNDCTL_DSP_MAPOUTBUF);
    _EXPORT_INT(m, SNDCTL_DSP_NONBLOCK);
    _EXPORT_INT(m, SNDCTL_DSP_POST);
    _EXPORT_INT(m, SNDCTL_DSP_PROFILE);
    _EXPORT_INT(m, SNDCTL_DSP_RESET);
    _EXPORT_INT(m, SNDCTL_DSP_SAMPLESIZE);
    _EXPORT_INT(m, SNDCTL_DSP_SETDUPLEX);
    _EXPORT_INT(m, SNDCTL_DSP_SETFMT);
    _EXPORT_INT(m, SNDCTL_DSP_SETFRAGMENT);
    _EXPORT_INT(m, SNDCTL_DSP_SETSPDIF);
    _EXPORT_INT(m, SNDCTL_DSP_SETSYNCRO);
    _EXPORT_INT(m, SNDCTL_DSP_SETTRIGGER);
    _EXPORT_INT(m, SNDCTL_DSP_SPEED);
    _EXPORT_INT(m, SNDCTL_DSP_STEREO);
    _EXPORT_INT(m, SNDCTL_DSP_SUBDIVIDE);
    _EXPORT_INT(m, SNDCTL_DSP_SYNC);
    _EXPORT_INT(m, SNDCTL_FM_4OP_ENABLE);
    _EXPORT_INT(m, SNDCTL_FM_LOAD_INSTR);
    _EXPORT_INT(m, SNDCTL_MIDI_INFO);
    _EXPORT_INT(m, SNDCTL_MIDI_MPUCMD);
    _EXPORT_INT(m, SNDCTL_MIDI_MPUMODE);
    _EXPORT_INT(m, SNDCTL_MIDI_PRETIME);
    _EXPORT_INT(m, SNDCTL_SEQ_CTRLRATE);
    _EXPORT_INT(m, SNDCTL_SEQ_GETINCOUNT);
    _EXPORT_INT(m, SNDCTL_SEQ_GETOUTCOUNT);
    _EXPORT_INT(m, SNDCTL_SEQ_GETTIME);
    _EXPORT_INT(m, SNDCTL_SEQ_NRMIDIS);
    _EXPORT_INT(m, SNDCTL_SEQ_NRSYNTHS);
    _EXPORT_INT(m, SNDCTL_SEQ_OUTOFBAND);
    _EXPORT_INT(m, SNDCTL_SEQ_PANIC);
    _EXPORT_INT(m, SNDCTL_SEQ_PERCMODE);
    _EXPORT_INT(m, SNDCTL_SEQ_RESET);
    _EXPORT_INT(m, SNDCTL_SEQ_RESETSAMPLES);
    _EXPORT_INT(m, SNDCTL_SEQ_SYNC);
    _EXPORT_INT(m, SNDCTL_SEQ_TESTMIDI);
    _EXPORT_INT(m, SNDCTL_SEQ_THRESHOLD);
    _EXPORT_INT(m, SNDCTL_SYNTH_CONTROL);
    _EXPORT_INT(m, SNDCTL_SYNTH_ID);
    _EXPORT_INT(m, SNDCTL_SYNTH_INFO);
    _EXPORT_INT(m, SNDCTL_SYNTH_MEMAVL);
    _EXPORT_INT(m, SNDCTL_SYNTH_REMOVESAMPLE);
    _EXPORT_INT(m, SNDCTL_TMR_CONTINUE);
    _EXPORT_INT(m, SNDCTL_TMR_METRONOME);
    _EXPORT_INT(m, SNDCTL_TMR_SELECT);
    _EXPORT_INT(m, SNDCTL_TMR_SOURCE);
    _EXPORT_INT(m, SNDCTL_TMR_START);
    _EXPORT_INT(m, SNDCTL_TMR_STOP);
    _EXPORT_INT(m, SNDCTL_TMR_TEMPO);
    _EXPORT_INT(m, SNDCTL_TMR_TIMEBASE);
}
