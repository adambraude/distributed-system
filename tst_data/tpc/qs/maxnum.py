import sys, re

f = open(sys.argv[1])
m = 0
for line in f:
    for k in re.findall(r'\d+', line):
        m = max(m,int(k))
print(m)
f.close()
