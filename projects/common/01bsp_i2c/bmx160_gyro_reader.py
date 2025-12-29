import time
from collections import deque

import pygame
import serial
import struct
from pygame.locals import *
from OpenGL.GL import *
from OpenGL.GLU import *

# need to update according to local serial device
SERIAL_PORT = '/dev/ttyACM1'
SERIAL_BAUDRATE = 115200

GYRO_RESOLUTION_LSB_PER_DPS = 32.8  # BMX160: ±500 dps range
GYRO_FILTER_ALPHA = 0.25            # exponential smoothing factor
CALIBRATION_SAMPLES = 200           # samples to average for bias removal
DISPLAY_FPS = 60

long  = 1
width = 0.8
hight = 0.2

# Cuboid vertices, edges and faces
vertices = [
    [long, -width, -hight], [long, width, -hight], [-long, width, -hight], [-long, -width, -hight],
    [long, -width, hight], [long, width, hight], [-long, width, hight], [-long, -width, hight]
]

face_edges = (
    (0, 1, 2, 3),
    (4, 5, 6, 7),
    (0, 4, 7, 3),
    (1, 5, 6, 2),
    (0, 1, 5, 4),
    (3, 2, 6, 7)
)

line_edges = [
    [0, 1], [1, 2], [2, 3], [3, 0],
    [0, 4], [1, 5], [2, 6], [3, 7],
    [4, 5], [5, 6], [6, 7], [7, 4]
]

blue_gray   = (0.68, 0.84, 1)
line_white  = (1,1,1)

face_colors = [blue_gray for i in range(6)]

def draw_cube():
    glBegin(GL_QUADS)
    for face in range(len(face_edges)):
        glColor3fv(face_colors[face])
        for vertex in face_edges[face]:
            glVertex3fv(vertices[vertex])
    glEnd()
    
    glColor3f(line_white[0], line_white[1], line_white[2])
    glBegin(GL_LINES)
    for edge in line_edges:
        for vertex in edge:
            glVertex3fv(vertices[vertex])
    glEnd()

def main():
    pygame.init()
    display = (800, 600)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)

    gluPerspective(45, (display[0] / display[1]), 0.1, 50.0)
    glTranslatef(0.0, 0.0, -5)

    # Connect to the serial port
    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE, timeout=0.01)

    raw_buffer = bytearray()
    clock = pygame.time.Clock()
    last_sample_time = time.perf_counter()
    smoothed_rate = [0.0, 0.0, 0.0]
    angles = [0.0, 0.0, 0.0]
    bias_samples = deque(maxlen=CALIBRATION_SAMPLES)
    bias = [0.0, 0.0, 0.0]
    calibrated = False

    while True:
        dt_frame = clock.tick(DISPLAY_FPS) / 1000.0
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                quit()

        # Read any pending bytes from the serial port
        in_waiting = ser.in_waiting or 1
        data = ser.read(in_waiting)
        if data:
            raw_buffer.extend(data)

        # Parse complete frames ending with CR LF
        while len(raw_buffer) >= 8:
            newline_index = raw_buffer.find(b"\r\n")
            if newline_index == -1:
                # keep buffer from growing indefinitely
                if len(raw_buffer) > 64:
                    raw_buffer = raw_buffer[-64:]
                break

            frame = raw_buffer[:newline_index]
            raw_buffer = raw_buffer[newline_index + 2:]

            if len(frame) != 6:
                continue

            x_raw, y_raw, z_raw = struct.unpack('>hhh', frame)
            now = time.perf_counter()
            dt_sample = max(now - last_sample_time, 1e-3)
            last_sample_time = now

            # convert raw data to deg/s
            rates = [axis / GYRO_RESOLUTION_LSB_PER_DPS for axis in (x_raw, y_raw, z_raw)]

            if not calibrated:
                bias_samples.append(rates)
                if len(bias_samples) == CALIBRATION_SAMPLES:
                    bias = [sum(samples[i] for samples in bias_samples) / CALIBRATION_SAMPLES for i in range(3)]
                    calibrated = True
                continue

            rates = [rates[i] - bias[i] for i in range(3)]
            smoothed_rate = [
                GYRO_FILTER_ALPHA * rates[i] + (1 - GYRO_FILTER_ALPHA) * smoothed_rate[i]
                for i in range(3)
            ]
            angles = [angles[i] + smoothed_rate[i] * dt_sample for i in range(3)]

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        glTranslatef(0.0, 0.0, -5)
        glRotatef(angles[0], 1, 0, 0)
        glRotatef(angles[1], 0, 1, 0)
        glRotatef(angles[2], 0, 0, 1)
        draw_cube()
        pygame.display.flip()

    # Close the serial port when finished
    ser.close()

if __name__ == '__main__':
    main()