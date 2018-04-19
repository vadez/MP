__kernel void normalization(__global uchar *disp_map,
	int width,
	int height,
	int max_val)
{
	const int i = get_global_id(0);

	int min_val = 0;

	disp_map[i] = (unsigned char)(255 * (disp_map[i] - min_val) / (max_val - min_val));
}