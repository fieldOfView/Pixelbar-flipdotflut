#!/usr/bin/python3

import socket
import random
import time

WIDTH = 112
HEIGHT = 16


def drawDot(sock, destination, column, row, polarity):
    """
    UDP packet format: Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)
    CMDH = 1CCC CCCC
    CMDL = 0xxP RRRR

    C = column address
    R = row address
    P = dot polarity (1= on/ 0=off)
    x = reserved for future use, set to 0 for now
    """

    cmdl = polarity << 4 | (row & 0x0F)
    cmdh = (1 << 7) | (column & 0x7F)

    sock.sendto(bytes([cmdh, cmdl]), destination)


def sendPattern(args):
    ip = socket.gethostbyname(args.host)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        if args.pattern == "fill":
            polarity = 1
        elif args.pattern == "clear":
            polarity = 0

        for iteration in range(0, args.repeat):
            for row in range(HEIGHT):
                if args.pattern == "rows":
                    polarity = row % 2
                for column in range(WIDTH):
                    if args.pattern == "columns":
                        polarity = column % 2
                    elif args.pattern == "random":
                        polarity = random.getrandbits(1)

                    drawDot(sock, (ip, args.port), column, row, polarity)

                    if args.debug:
                        print(chr(0x2588) if polarity else " ", end="")
                if args.debug:
                    print("")
                if not args.nodelay:
                    time.sleep(0.001)
            if args.debug and args.repeat > 1 and iteration < args.repeat - 1:
                print("\033[F" * (HEIGHT + 1))


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1337)
    parser.add_argument(
        "pattern",
        choices=["fill", "clear", "rows", "columns", "random"],
        default="fill",
    )
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--nodelay", action="store_true")
    parser.add_argument("--debug", action="store_true")

    args = parser.parse_args()
    sendPattern(args)
