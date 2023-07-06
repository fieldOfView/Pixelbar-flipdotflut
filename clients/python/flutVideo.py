#!/usr/bin/python3

import cv2
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


def sendVideo(args):
    ip = socket.gethostbyname(args.host)

    cap = cv2.VideoCapture(args.video)
    if not cap.isOpened():
        return

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        screen_ratio = WIDTH / HEIGHT
        resampling = cv2.INTER_AREA

        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                break

            frame_height, frame_width = frame.shape[:2]

            if args.scale == "stretch":
                frame = cv2.resize(frame, (WIDTH, HEIGHT), interpolation=resampling)
            else:
                if args.scale in ["fill", "fit"]:
                    image_ratio = frame_width / frame_height

                    if (args.scale == "fill" and image_ratio > screen_ratio) or (
                        args.scale == "fit" and image_ratio < screen_ratio
                    ):
                        size = (math.floor(HEIGHT * image_ratio), HEIGHT)
                    else:
                        size = (WIDTH, math.floor(WIDTH / image_ratio))
                    frame = cv2.resize(frame, size, interpolation=resampling)

                frame_height, frame_width = frame.shape[:2]
                left = math.floor((frame_width - WIDTH) / 2)
                top = math.floor((frame_height - HEIGHT) / 2)
                frame = frame[top:(top + HEIGHT), left:(left + WIDTH)]
                # NB: frame could now be smaller than WIDTH, HEIGHT!
                # see extension below (after conversion to 1 bit)

            # covert to greyscale
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            # convert to bitmap
            (thresh, frame) = cv2.threshold(frame, 127, 255, cv2.THRESH_BINARY)

            if args.invert:
                frame = cv2.bitwise_not(frame)

            frame_height, frame_width = frame.shape[:2]
            if frame_width < WIDTH or frame_height < HEIGHT:
                left = math.floor((WIDTH - frame_width)/2)
                right = WIDTH - frame_width - left
                top = math.floor((HEIGHT - frame_height)/2)
                bottom = HEIGHT - frame_height - top
                frame = cv2.copyMakeBorder(frame, top, bottom, left, right, cv2.BORDER_CONSTANT, 0)

            for row in range(HEIGHT):
                for column in range(WIDTH):
                    polarity = 1 if frame[row, column] else 0
                    drawDot(sock, (ip, args.port), column, row, polarity)

                    if args.debug:
                        print(chr(0x2588) if polarity else " ", end="")
                if args.debug:
                    print("")

            if args.debug:
                print("\033[F" * (HEIGHT + 1))

            cv2.waitKey(25)
            #cv2.imshow("Frame", frame)

            # Press Q on keyboard to exit
            #if cv2.waitKey(25) & 0xFF == ord('q'):
            #    break


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
    parser.add_argument("video")
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--debug", action="store_true")

    args = parser.parse_args()

    sendVideo(args)
