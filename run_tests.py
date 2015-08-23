
import time
import sys
import os

IS_NUMA = 1
IS_2_SOCKET = 0
IS_PERF = 0

CMD_PARAMS = '-a -w%d -b%d -d%d -u%d -i%d -r%d -n%d'

PERF_FILE = "__perf_output.file"
CMD_PREFIX_PERF = "perf stat -d -o %s" % (PERF_FILE,)

CMD_BASE_HARRIS = './bench-harris'
CMD_BASE_HP_HARRIS = './bench-hp-harris'
CMD_BASE_RCU = './bench-rcu'
CMD_BASE_RLU = './bench-rlu'

OUTPUT_FILENAME = '___temp.file'

W_OUTPUT_FILENAME = '__w_check.txt'

CMD_PREFIX_LIBS = 'export LD_PRELOAD=\\"$GPERFTOOLS_LIB/libtcmalloc_minimal.so\\"'

CMD_NUMA_BIND_TO_CPU_0 = 'numactl --cpunodebind=0 '
CMD_NUMA_BIND_TO_CPU_1 = 'numactl --cpunodebind=1 '
CMD_NUMA_BIND_TO_CPU_0_1 = 'numactl --cpunodebind=0,1 '
CMD_NUMA_PREFIX_8  = 'taskset -c 0-7 '
CMD_NUMA_PREFIX_10 = 'taskset -c 0-9 '
CMD_NUMA_PREFIX_12 = 'taskset -c 0-11 '
CMD_NUMA_PREFIX_14 = 'taskset -c 0-13 '
CMD_NUMA_PREFIX_16 = 'taskset -c 0-15 '

CMD_BASE = {
	'harris' : CMD_BASE_HARRIS,
	'hp_harris' : CMD_BASE_HP_HARRIS,
	'rcu' : CMD_BASE_RCU,
	'rlu' : CMD_BASE_RLU,
}

result_keys = [
	'#ops          :',
	'#update ops   :',
	't_writer_writebacks =',
	't_writeback_q_iters =',
	'a_writeback_q_iters =',
	't_pure_readers =',
	't_steals =',
	't_aborts =',
	't_sync_requests =',
	't_sync_and_writeback =',
]

perf_result_keys = [
	'instructions              #',
	'branches                  #',
	'branch-misses             #',
	'L1-dcache-loads           #',
	'L1-dcache-load-misses     #',
]

def cmd_numa_prefix(threads_num):
	if (IS_2_SOCKET):
		if (threads_num <= 36):
			return CMD_NUMA_BIND_TO_CPU_1
		
		return CMD_NUMA_BIND_TO_CPU_0_1
	
	if (threads_num <= 8):
		return CMD_NUMA_PREFIX_8
	
	if (threads_num <= 10):
		return CMD_NUMA_PREFIX_10

	if (threads_num <= 12):
		return CMD_NUMA_PREFIX_12

	if (threads_num <= 14):
		return CMD_NUMA_PREFIX_14

	if (threads_num <= 16):
		return CMD_NUMA_PREFIX_16

	print 'cmd_numa_prefix: ERROR th_num = %d' % (threads_num,)


def extract_data(output_data, key_str):
	data = output_data.split(key_str)[1].split()[0].strip()
	
	if (data.find('nan') != -1):
		return 0
	
	if (key_str == 'L1-dcache-load-misses     #') or (key_str == 'branch-misses             #'):
		data = data.strip('%')
			
	return float(data)

	
def extract_keys(output_data):
	d = {}
	
	for key in result_keys:
		d[key] = extract_data(output_data, key)
	
	if IS_PERF:
		for key in perf_result_keys:
			d[key] = extract_data(output_data, key)
			
	return d


def print_keys(dict_keys):
	print '================================='
	for key in result_keys:
		print '%s %.2f' % (key, dict_keys[key])
	
	if IS_PERF:
		for key in perf_result_keys:
			print '%s %.2f' % (key, dict_keys[key])
	

def run_test(runs_per_test, alg_type, cmd):
  
	ops_total = 0
	total_operations = 0
	aborts_total = 0
	total_combiners = 0
	total_num_of_waiting = 0
	total_additional_readers = 0
	total_additional_writers = 0

	cmd_prefix = 'bash -c "' + CMD_PREFIX_LIBS + ' ; '
	if (IS_PERF):
		cmd_prefix += CMD_PREFIX_PERF + ' '
	
	full_cmd = cmd_prefix + cmd + '"'
	
	print full_cmd
	
	total_dict_keys = {}
	for key in result_keys:
		total_dict_keys[key] = 0
	
	for i in xrange(runs_per_test):
		print 'run %d ' % (i,)
		
		
		if (IS_PERF):
			try:
				os.unlink(PERF_FILE)
			except OSError:
				pass
		
		os.system('w >> %s' % (W_OUTPUT_FILENAME,))
		time.sleep(1)
		os.system(full_cmd + ' > ' + OUTPUT_FILENAME)
		os.system('w >> %s' % (W_OUTPUT_FILENAME,))

		time.sleep(1)
		f = open(OUTPUT_FILENAME, 'rb')
		output_data = f.read()
		f.close()
		os.unlink(OUTPUT_FILENAME)
		
		if (IS_PERF):
			f = open(PERF_FILE, 'rb');
			output_data += f.read()
			f.close()
			os.unlink(PERF_FILE)
		
		print "------------------------------------"
		print output_data
		print "------------------------------------"
				
		dict_keys = extract_keys(output_data)

		print '================================='
		for key in dict_keys.keys():
			total_dict_keys[key] += dict_keys[key]
			
		print_keys(dict_keys)
		
	for key in total_dict_keys.keys():
		total_dict_keys[key] /= runs_per_test
 	
	return total_dict_keys

def print_run_results(f_out, rlu_max_ws, update_ratio, th_num, dict_keys):
	
	f_out.write('\n%.2f %.2f %.2f' % (rlu_max_ws, update_ratio, th_num));
	
	for key in result_keys:
		f_out.write(' %.2f' % dict_keys[key])
	
	if IS_PERF:
		for key in perf_result_keys:
			f_out.write(' %.2f' % dict_keys[key])
	
	f_out.flush()

	
def execute(runs_per_test, 
			rlu_max_ws,
			buckets,
			duration, 
			alg_type, 
			update_ratio, 
			initial_size, 
			range_size, 
			output_filename, 
			th_num_list):
  
	
	f_w = open(W_OUTPUT_FILENAME, 'wb');
	f_w.close()

	f_out = open(output_filename, 'wb')
	
	cmd_header = '[%s] ' % (alg_type,) + CMD_BASE[alg_type] + ' ' + CMD_PARAMS % (
		rlu_max_ws,
		buckets,
		duration,
		update_ratio, 
		initial_size,
		range_size,	
		0)
		
	f_out.write(cmd_header + '\n')
	f_out.flush()
	
	results = []
	for th_num in th_num_list:
		
		
		cmd = CMD_BASE[alg_type] + ' ' + CMD_PARAMS % (
			rlu_max_ws,
			buckets,
			duration,
			update_ratio, 
			initial_size,
			range_size,	
			th_num)
		
		if (IS_NUMA):
			cmd = cmd_numa_prefix(th_num) + cmd
			
		print '-------------------------------'
		print '[%d] %s ' % (th_num, cmd)

		dict_keys = run_test(runs_per_test, alg_type, cmd)

		results.append(dict_keys)

		print_run_results(f_out, rlu_max_ws, update_ratio, th_num, dict_keys)

	
	f_out.write('\n\n')
	f_out.flush()
	f_out.close()
	
	
	print 'DONE: written output to %s' % (output_filename,)


if '__main__' == __name__:
	try:

		param_num = 10
		
		prog_name, runs_per_test, rlu_max_ws, buckets, duration, alg_type, update_ratio, initial_size, range_size, output_filename = sys.argv[:param_num]
		th_num_list = sys.argv[param_num:]

		runs_per_test = int(runs_per_test)
		rlu_max_ws = int(rlu_max_ws)
		buckets = int(buckets)
		duration = int(duration)
		update_ratio = int(update_ratio)
		initial_size = int(initial_size)
		range_size = int(range_size)
		for i, th_num in enumerate(th_num_list):
			th_num_list[i] = int(th_num)

	except ValueError, e:
		raise e
		sys.exit('USAGE: %s [runs_per_test] [rlu_max_ws] [buckets] [duration] [alg_type] [update_ratio] [initial_size] [range_size] [output_filename] [[thread num list]]' % (sys.argv[0],))


	execute(runs_per_test, rlu_max_ws, buckets, duration, alg_type, update_ratio, initial_size, range_size, output_filename, th_num_list)

