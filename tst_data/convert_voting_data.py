#!/usr/bin/python3

"""
Running
    python3 convert_voting_data.py
will create the `./voting_data/` directory and fill it with LITERAL WAH
vectors created by parsing the `voting_data.tsv` file.

The vectors are explained in the generated `README.md` in the directory.
"""

from math import ceil
import pathlib

# Vector word length.
word_length = 64

# Create output directory.
output_dir = "./voting_data"
pathlib.Path(output_dir).mkdir(exist_ok=True)

# File in tsv format to convert.
input_file = "voting_data.tsv"

# Vector options.
vote_options = { 0 : "+", 1 : "-", 2 : "." }

# Each row has 1 affiliation with 2 options, and 10 votes with 3 options.
num_vectors = (1 * 2) + (10 * 3)

# The number of rows in the input file.
num_rows = sum(1 for row in open(input_file))

# The number of words needed to hold the number of rows
num_words = int(ceil(float(num_rows) / word_length))

# The length of the vectors (in bits).
vector_len = num_words * word_length

vectors = [[None for j in range(vector_len)] for i in range(num_vectors)]

# Convert a list of boolean values into a binary string.
def boolean_list_to_hex_string(array):
    bin_str = "".join(["1" if x == True else "0" for x in array])
    #value = int(bin_str, 2)
    # vector length is in bits, so div by 4 for number of hex, add 2 for 0x.
    #hex_width = 2 + vector_len // 4
    #bin_str = bin(value).ljust(hex_width, "0").replace("0x", "")
    s = [hex(int('0' + bin_str[i:i+63], 2)) for i in range(0, len(bin_str), 63)]
    return "\n".join(s)

def process_row(row, row_num):
    attrs = row.split()
    # attrs[0] -> Identifier
    ident = attrs[0]
    # attrs[1] -> Affiliation [D|R]
    affil = attrs[1]
    # attrs[2] -> Votes [+|-|.]*10
    votes = attrs[2]

    vectors[0][row_num] = (affil == "D")
    vectors[1][row_num] = (affil == "R")

    for vector in range(2, num_vectors):
        vote_num = (vector - 2) // 3
        vote_val = (vector - 2) % 3

        vectors[vector][row_num] = (votes[vote_num] == vote_options[vote_val])

with open(input_file) as file:
    i = 0
    for row in file:
        process_row(row, i)
        i += 1

vec_id = 0
for vector in vectors:
    filename = output_dir + "/v_{vec_id}.dat".format(vec_id=vec_id)
    with open(filename, "w") as text_file:
        text_file.write(boolean_list_to_hex_string(vector) + "\n")
    vec_id += 1

readme ="""# Voting Data Vectors

**Important** each vector is a literal 64-bit WAH

The vectors are broken down as follows:

```
v_0.dat -> Democrat
v_1.dat -> Republican

v_2.dat -> vote "+" on 1
v_3.dat -> vote "-" on 1
v_4.dat -> vote "." on 1

v_5.dat -> vote "+" on 2
v_6.dat -> vote "-" on 2
v_7.dat -> vote "." on 2

...

v_29.dat -> vote "+" on 10
v_30.dat -> vote "-" on 10
v_31.dat -> vote "." on 10
```

The *n*th row corresponds to the *n+1*th representative in the original file.
"""

readme_file = output_dir + "/README.md"
with open(readme_file, "w") as text_file:
    text_file.write(readme)
