#!/usr/bin/python3

from PIL import Image
import PIL.ImageOps
import socket
import math
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


def sendImage(args):
    ip = socket.gethostbyname(args.host)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        radial = Image.radial_gradient("L")
        if not args.invert:
            radial = PIL.ImageOps.invert(radial)

        image = Image.new("L", (WIDTH, HEIGHT))
        while True:
            x = math.floor((WIDTH-256)/2 * (1+math.sin(time.time())))
            y = math.floor((HEIGHT-256)/2 * (1+math.cos(time.time()/2.3245)))

            box = (x, y, x+256, y+256)
            image.paste(radial, box)

            image = image.convert("1")

            for row in range(HEIGHT):
                for column in range(WIDTH):
                    polarity = 1 if image.getpixel((column, row)) else 0
                    drawDot(sock, (ip, args.port), column, row, polarity)

                    if args.debug:
                        print(chr(0x2588) if polarity else " ", end="")
                if args.debug:
                    print("")
            if not args.nodelay:
                time.sleep(0.01)
            if args.debug:
                print("\033[F" * (HEIGHT+1))

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1337)
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--nodelay", action="store_true")
    parser.add_argument("--debug", action="store_true")

    args = parser.parse_args()

    sendImage(args)
