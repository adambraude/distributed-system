#!/usr/bin/python # this really should be python3
"""
    Assumes that the vec/ directory already exists
"""
import ast
import sys
sys.path.insert(0, '..') # include top-level test utility file
import tst_util
worldfile = None
try:
    worldfile = open(sys.argv[1], encoding="latin-1")
except:
    print("Usage python3 parse-world.py worldfile")
ids = set({})
cities = set({})
codes = set({})
districts = set({})
pops = set({})
citystr = "INSERT INTO `city` VALUES "
tuples = []
countrystr = "INSERT INTO `country` VALUES "
for line in worldfile:
    if citystr in line:
        string_tuple = line.split(citystr)[1]
        tup = ast.literal_eval(string_tuple.replace(';', ''))
        ids.add(tup[0])
        cities.add(tup[1])
        codes.add(tup[2])
        districts.add(tup[3])
        pops.add(tup[4])
        tuples.append(tup)

print("IDs: {0} City Names: {1} Codes: {2} Districts: {3} Pops: {4}".format(len(ids), len(cities), len(codes), len(districts), len(pops)))
# we don't need to include IDs in the bitmap since that's just the row number
vector_len = max([len(ids), len(cities), len(codes), len(districts), len(pops)])
city_vectors = [[0 for i in range(vector_len)] for j in range(len(cities))]

city_list = sorted(list(cities)) # sorting needed to get consistent vector ID's for each city
# create city vectors
for tup in tuples:
    city_vectors[city_list.index(tup[1])][tup[0] - 1] = 1

for city_ix, city in enumerate(city_vectors):
    f = open("vec/v_{0}.dat".format(city_ix), "w")
    for v in tst_util.vecstrings(city):
        f.write('{0}\n'.format(v))
    f.close()
offset = city_ix

worldfile.close()
