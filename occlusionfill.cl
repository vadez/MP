__kernel void occlusionfill(__global uchar *disp_map,
	__global uchar *disp_map_OF,
	int width,
	int height,
	int win_width,
	int win_height)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);

	int sum_win;
	int pixel_count;

	if (disp_map[y*width + x] == 0)
	{
		sum_win = 0;
		pixel_count = 0;

		for (int y_win = -win_height; y_win <= win_height; y_win++)
		{
			for (int x_win = -win_width; x_win <= win_width; x_win++)
			{
				if (!(y + y_win >= 0) ||
					!(y + y_win < height) ||
					!(x + x_win >= 0) ||
					!(x + x_win < width))
				{
					continue;
				}
				if (disp_map[y*width + y_win + x + x_win] != 0)
				{
					sum_win += disp_map[y*width + y_win + x + x_win];
					pixel_count++;
				}
			}
		}
		if (pixel_count > 0)
		{
			
			disp_map_OF[y*width + x] = sum_win / pixel_count;
		}
	}
	else
	{
		disp_map_OF[y*width + x] = disp_map[y*width + x];
	}
}
