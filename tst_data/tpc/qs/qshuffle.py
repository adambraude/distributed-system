import sys
from random import shuffle
f = open(sys.argv[1])
qs = [line for line in f]
shuffle(qs)
f.close()
f = open(".{}".format(sys.argv[1].replace(".dat", ".shuffled.dat")), "w")
for q in qs: f.write(q)
f.close()
