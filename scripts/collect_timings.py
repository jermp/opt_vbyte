import sys, os

path_to_basename = sys.argv[1] # e.g., '/data2/inverted_indexes/gov2/gov2.sorted-text'
path_to_binaries = sys.argv[2] # e.g., './bin'
path_to_results = sys.argv[3]  # e.g., './results'
prefix_name = sys.argv[4]      # e.g., 'gov2'
query_log = sys.argv[5]

index_types = ["opt_vb_dp", "uniform_vb", "opt_vb",
               "block_maskedvbyte", "block_streamvbyte",
               "block_varintgb", "block_varintg8iu"]

for type in index_types:
        cmd = "./drop_caches; ./queries " + type + " and " + path_to_binaries + "/" + prefix_name + "." + type + ".bin " + path_to_basename + "." + query_log + " > " + path_to_results + "/" + prefix_name + "." + type + ".querytime." + query_log
        # print cmd
        for i in xrange(0, 3):
            os.system(cmd)

# for type in index_types:
#     print
#     cmd = "./drop_caches; ./index_perftest " + type + " " + path_to_binaries + "/" + prefix_name + "." + type + ".bin > " + path_to_results + "/" + prefix_name + "." + type + ".sequential_decoding"
#     print cmd
#     for i in xrange(0, 5):
#         os.system(cmd)
