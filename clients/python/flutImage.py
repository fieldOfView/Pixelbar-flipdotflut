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
    with Image.open(args.image) as image, socket.socket(
        socket.AF_INET, socket.SOCK_DGRAM
    ) as sock:
        resampling = Image.Resampling.BICUBIC
        dither = Image.Dither.NONE if args.nodither else Image.Dither.FLOYDSTEINBERG

        if args.scale == "stretch":
            image = image.resize((WIDTH, HEIGHT), resampling)
        else:
            if args.scale in ["fill", "fit"]:
                image_ratio = image.width / image.height
                screen_ratio = WIDTH / HEIGHT
                if (args.scale == "fill" and image_ratio > screen_ratio) or (
                    args.scale == "fit" and image_ratio < screen_ratio
                ):
                    size = (math.floor(HEIGHT * image_ratio), HEIGHT)
                else:
                    size = (WIDTH, math.floor(WIDTH / image_ratio))
                image = image.resize(size, resampling)

            left = (image.width - WIDTH) / 2
            top = (image.height - HEIGHT) / 2

            image = image.crop((left, top, left + WIDTH, top + HEIGHT))

        image = image.convert("1", dither=dither)

        if args.invert:
            image = PIL.ImageOps.invert(image)

        for iterate in range(0, args.iterations):
            for row in range(HEIGHT):
                for column in range(WIDTH):
                    polarity = 1 if image.getpixel((column, row)) else 0
                    drawDot(sock, (ip, args.port), column, row, polarity)

                    if args.debug:
                        print(chr(0x2588) if polarity else " ", end="")
                if args.debug:
                    print("")
                if not args.nodelay:
                    time.sleep(0.001)

            if args.debug and args.iterations > 1 and iterate < args.iterations - 1:
                print("\033[F" * (HEIGHT + 1))


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1337)
    parser.add_argument(
        "--scale",
        choices=["stretch", "fill", "fit", "crop"],
        default="fill",
    )
    parser.add_argument("image")
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--nodither", action="store_true")
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--nodelay", action="store_true")
    parser.add_argument("--debug", action="store_true")

    args = parser.parse_args()

    sendImage(args)
