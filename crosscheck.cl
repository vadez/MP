__kernel void crosscheck(__global uchar *disp_map1,
	__global uchar *disp_map2,
	__global uchar *disp_map_CC,
	int threshold)
{
	const int i = get_global_id(0);

	if (abs((int)disp_map1[i] - disp_map2[i]) > threshold)
	{
		// If absolute value of L and R depthmaps differ by treshold
		// Set disparity to 0
		disp_map_CC[i] = 0;
	}
	else
	{
		disp_map_CC[i] = disp_map1[i];
	}
}