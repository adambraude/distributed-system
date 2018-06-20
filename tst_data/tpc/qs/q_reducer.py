import sys
f = open(sys.argv[1])
i = 1
m = 0
rmd = 0
f2 = open("out", "w+")
for line in f:
    t = line.split("&")
    if len(t) > 128:
        print("Line {}: len {}, removed ".format(i, len(t)))
        rmd += 1
    else:
        f2.write(line)
    i += 1
    m = max(m,len(t))
print("Max = {}, rmd = {}".format(m, rmd))
f.close()
f2.close()
testf = open("out")
for line in testf:
    assert(len(line.split('&')) <= 128)
testf.close()
