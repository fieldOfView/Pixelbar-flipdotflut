"""
   * UDP packet format: Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)
   * CMDH = 1CCC CCCC
   * CMDL = 0xxP RRRR
   *
   * C = column address
   * R = row address
   * P = dot polarity (1= on/ 0=off)
   * x = reserved for future use, set to 0 for now
"""

import socket
import random

WIDTH = 112
HEIGHT = 16

def drawDot(sock, destination, column, row, polarity):
    cmdl = polarity << 4 | (row & 0x0F)
    cmdh = (1 << 7) | (column & 0x7F)

    sock.sendto(bytes([cmdh, cmdl]), destination)


def sendPattern(pattern, destination, debug=False):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        if pattern == "fill":
            polarity = 1
        elif pattern == "clear":
            polarity = 0

        for row in range(HEIGHT):
            if pattern == "rows":
                polarity = row % 2
            for column in range(WIDTH):
                if pattern == "columns":
                    polarity = column % 2
                elif pattern == "random":
                    polarity = random.getrandbits(1)

                drawDot(sock, destination, column, row, polarity)

                if debug:
                    print(chr(0x2588) if polarity else " ", end="")
            if debug:
                print("")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="127.0.0.1")
    parser.add_argument("--port", default=1234)
    parser.add_argument(
        "pattern",
        choices=["fill", "clear", "rows", "columns", "random"],
        default="fill",
    )
    parser.add_argument("--debug", action='store_true')

    args = parser.parse_args()

    sendPattern(args.pattern, (args.ip, args.port), args.debug)
