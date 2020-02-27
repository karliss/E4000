#!/bin/env python3

import csv


def read_table(name):
    with open(name, 'r') as csvfile:
        reader = csv.DictReader(csvfile)
        result = list(reader)
    return result

data = read_table('table1.csv')


COLS = 8
ROWS = 24

def map_row_v1(pin):
    mapping = {
        1: 7,
        2: 0,
        3: 1,
        4: 2,
        5: 3,
        17: 4,
        18: 13,
        19: 23,
        20: 9,
        21: 10,
        22: 11,
        23: 16,
        24: 17,
        25: 18,
        26: 19,
        27: 20,
        28: 21,
        29: 22,
        30: 23,
        31: 12,
        32: 13,
        33: 14,
        34: 15,
        35: 8
    }
    return mapping[pin]

tb = []
for i in range(ROWS):
    tb.append([None] * COLS)
for row in data:
    tb[map_row_v1(int(row["Bottom"]))][int(row["T0"])-1] = row

#    print(",".join(map(lambda x: x['name'] if x else "",row)))


def print_normid():
    print("{")
    for row in tb:
        line = "{"
        line += ",".join(map(lambda x: str(x['normalid']) if x and x['normalid'] else '0', row))
        line += "},"
        print(line)
    print("}")

print_normid()
