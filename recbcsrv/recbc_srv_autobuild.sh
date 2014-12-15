#!/bin/bash
gcc -O -Wall -D_FILE_OFFSET_BITS=64 -I./PC_CENTRIST/ recbc_srv.c datafifo.c ./PC_CENTRIST/CtImage.c ./PC_CENTRIST/svm.c ./PC_CENTRIST/DetectFrame.c ./PC_CENTRIST/FrameDiff.c xmlparser.c -o recbc_srv -lpthread -lm libjpeg.a libmxml.a

