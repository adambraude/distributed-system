# literal vector testing
# assumes all vectors are literals of the same length
import sys
input_str = sys.argv[1] # of the form 'R:[v0,v1]&...{&[vn,vm]}'
ranges = input_str.split('&')
final_results = []
for r in ranges:
    r = r.replace('R:', '').replace('[','').replace(']','').split(',')
    result_vectors = []
    print(r)
    for k in range(int(r[0]), int(r[1]) + 1):
        f = open("voting_data/v_" + str(k) + ".dat")
        result_vectors.append([int(s, 16) for s in f.read().split('\n') if s])
    res = result_vectors[0]
    for j in range(1, len(result_vectors)):
        res = [res[i] | result_vectors[j][i] for i in range(0, len(res))]
    final_results.append(res)
res = final_results[0]
for k in range(1, len(final_results)):
    res = [res[i] & final_results[k][i] for i in range(len(res))]
for subvec in res:
    print(hex(subvec))
