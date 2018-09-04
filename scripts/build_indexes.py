import sys, os

path_to_basename = sys.argv[1] # e.g., '/data2/inverted_indexes/gov2/gov2.sorted-text.bin'
path_to_binaries = sys.argv[2] # e.g., './bin'
path_to_results = sys.argv[3]  # e.g., './results'
prefix_name = sys.argv[4]      # e.g., 'gov2'

index_types = [
	"opt_vb_dp", "uniform_vb", "opt_vb",
    "block_maskedvbyte", "block_streamvbyte",
    "block_varintgb", "block_varintg8iu"
	]

for type in index_types:
    os.system("./drop_caches; ./create_freq_index " + type + " " + path_to_basename + " --out " + path_to_binaries + "/" + prefix_name + "." + type + ".bin > " + path_to_results + "/" + prefix_name + "." + type + ".stats")

