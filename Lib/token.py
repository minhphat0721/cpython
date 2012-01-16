"""Token constants (from "token.h")."""

__all__ = ['tok_name', 'ISTERMINAL', 'ISNONTERMINAL', 'ISEOF']

#  This file is automatically generated; please don't muck it up!
#
#  To update the symbols in this file, 'cd' to the top directory of
#  the python source tree after building the interpreter and run:
#
#    ./python Lib/token.py

#--start constants--
ENDMARKER = 0
NAME = 1
NUMBER = 2
STRING = 3
NEWLINE = 4
INDENT = 5
DEDENT = 6
LPAR = 7
RPAR = 8
LSQB = 9
RSQB = 10
COLON = 11
COMMA = 12
SEMI = 13
PLUS = 14
MINUS = 15
STAR = 16
SLASH = 17
VBAR = 18
AMPER = 19
LESS = 20
GREATER = 21
EQUAL = 22
DOT = 23
PERCENT = 24
LBRACE = 25
RBRACE = 26
EQEQUAL = 27
NOTEQUAL = 28
LESSEQUAL = 29
GREATEREQUAL = 30
TILDE = 31
CIRCUMFLEX = 32
LEFTSHIFT = 33
RIGHTSHIFT = 34
DOUBLESTAR = 35
PLUSEQUAL = 36
MINEQUAL = 37
STAREQUAL = 38
SLASHEQUAL = 39
PERCENTEQUAL = 40
AMPEREQUAL = 41
VBAREQUAL = 42
CIRCUMFLEXEQUAL = 43
LEFTSHIFTEQUAL = 44
RIGHTSHIFTEQUAL = 45
DOUBLESTAREQUAL = 46
DOUBLESLASH = 47
DOUBLESLASHEQUAL = 48
AT = 49
RARROW = 50
ELLIPSIS = 51
OP = 52
ERRORTOKEN = 53
N_TOKENS = 54
NT_OFFSET = 256
#--end constants--

tok_name = {value: name
            for name, value in globals().items()
            if isinstance(value, int)}
__all__.extend(tok_name.values())

def ISTERMINAL(x):
    return x < NT_OFFSET

def ISNONTERMINAL(x):
    return x >= NT_OFFSET

def ISEOF(x):
    return x == ENDMARKER


def _main():
    import re
    import sys
    args = sys.argv[1:]
    inFileName = args and args[0] or "Include/token.h"
    outFileName = "Lib/token.py"
    if len(args) > 1:
        outFileName = args[1]
    try:
        fp = open(inFileName)
    except IOError as err:
        sys.stdout.write("I/O error: %s\n" % str(err))
        sys.exit(1)
    lines = fp.read().split("\n")
    fp.close()
    prog = re.compile(
        "#define[ \t][ \t]*([A-Z0-9][A-Z0-9_]*)[ \t][ \t]*([0-9][0-9]*)",
        re.IGNORECASE)
    tokens = {}
    for line in lines:
        match = prog.match(line)
        if match:
            name, val = match.group(1, 2)
            val = int(val)
            tokens[val] = name          # reverse so we can sort them...
    keys = sorted(tokens.keys())
    # load the output skeleton from the target:
    try:
        fp = open(outFileName)
    except IOError as err:
        sys.stderr.write("I/O error: %s\n" % str(err))
        sys.exit(2)
    format = fp.read().split("\n")
    fp.close()
    try:
        start = format.index("#--start constants--") + 1
        end = format.index("#--end constants--")
    except ValueError:
        sys.stderr.write("target does not contain format markers")
        sys.exit(3)
    lines = []
    for val in keys:
        lines.append("%s = %d" % (tokens[val], val))
    format[start:end] = lines
    try:
        fp = open(outFileName, 'w')
    except IOError as err:
        sys.stderr.write("I/O error: %s\n" % str(err))
        sys.exit(4)
    fp.write("\n".join(format))
    fp.close()


if __name__ == "__main__":
    _main()
