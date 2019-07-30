#ifndef _SYNAPTICS_COMMON_INTERFACE_H_
#define _SYNAPTICS_COMMON_INTERFACE_H_

struct synatcm_tpd {
	char *synatcm_test_failed_node;
	char *synatcm_test_temp_buffer;
	char *synatcm_test_failed_node_buffer;
	int failed_node_count;
	int failed_node_buffer_len;
	int cols;
	int rows;
	int test_result;
};

#endif /* _SYNAPTICS_COMMON_INTERFACE_H_ */
