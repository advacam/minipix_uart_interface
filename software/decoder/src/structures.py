import numpy

# TODO update this according to real Timepix
MODE_TOA_TOT  = 10
MODE_TOA      = 7
MODE_MPX_ITOT = 4

# #{ class PixelDataToAToT

class PixelDataToAToT:

    def __init__(self):

        self.ftoa      = 0
        self.toa       = 0
        self.tot       = 0
        self.x         = 0
        self.y         = 0
        self.mode_mask = 0

# #} end of classPixelDataToAToT

# #{ class PixelDataToA

class PixelDataToA:

    def __init__(self):

        self.ftoa      = 0
        self.toa       = 0
        self.x         = 0
        self.y         = 0
        self.mode_mask = 0

# #} end of class PixelDataToA

# #{ class PixelDataMpxiToT

class PixelDataMpxiToT:

    def __init__(self):

        self.event_counter = 0
        self.itot          = 0
        self.x             = 0
        self.y             = 0
        self.mode_mask     = 0

# #} end of class PixelDataMpxiToT

# #{ class ImageToAToT

class ImageToAToT:

    def __init__(self):

        self.tot  = numpy.zeros(shape=[256, 256])
        self.toa  = numpy.zeros(shape=[256, 256])
        self.ftoa = numpy.zeros(shape=[256, 256])

# #} end of class Image

# #{ class ImageToA

class ImageToA:

    def __init__(self):

        self.toa  = numpy.zeros(shape=[256, 256])
        self.ftoa = numpy.zeros(shape=[256, 256])

# #} end of class Image

# #{ class ImageMpxiToT

class ImageMpxiToT:

    def __init__(self):

        self.event_counter = numpy.zeros(shape=[256, 256])
        self.itot          = numpy.zeros(shape=[256, 256])

# #} end of class Image

# #{ class FrameDataMsg

class FrameDataMsg:

    def __init__(self):

        self.frame_id         = 0
        self.packet_id        = 0
        self.mode             = 0
        self.n_pixels         = 0
        self.checksum_matched = 0

        self.pixel_data = []

# #} end of class FrameDataMsg
