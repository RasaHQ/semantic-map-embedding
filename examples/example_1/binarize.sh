#!/bin/bash

python3 ../../scripts/text_to_binary.py --corpus ./corpus --out example_1.bin --vocabulary ./vocab.txt
rm example_1.bin.part-*
