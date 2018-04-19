__kernel void zncc(__global uchar *in_img_1,
	__global uchar *in_img_2,
	__global uchar *disp_map,
	int width,
	int height,
	int win_size,
	int min_disp,
	int max_disp)
{

	const int x = get_global_id(0);
	const int y = get_global_id(1);

	float sum_of_window_values1;
	float sum_of_window_values2;
	float standart_deviation1;
	float standart_deviation2;
	float multistand = 0;
	int best_disparity_value;
	float current_max_sum;
	int d;
	int win_x, win_y;
	float var1, var2;
	int winhalf;
	winhalf = win_size >> 1;

	best_disparity_value = max_disp;
	current_max_sum = 0;
	for (d = min_disp; d <= max_disp; d++)
	{
		sum_of_window_values1 = sum_of_window_values2 = 0;
		for (win_y = -winhalf; win_y <winhalf; win_y++)
		{
			for (win_x = -winhalf; win_x < winhalf; win_x++)
			{
				if (y + win_y >= 0 &&
					x + win_x >= 0 &&
					y + win_y < height &&
					x + win_x < width &&
					x + win_x - d >= 0 &&
					x + win_x - d < width)
				{
					//Calculate the mean value for each window
					sum_of_window_values1 += in_img_1[(win_y + y)*width + (x + win_x)];
					sum_of_window_values2 += in_img_2[(win_y + y)*width + (x + win_x - d)];
				}
				else
				{
					continue;
				}
			}
		}
		sum_of_window_values1 /= (win_size*win_size);
		sum_of_window_values2 /= (win_size*win_size);

		var1 = 0;
		var2 = 0;
		multistand = 0;
		for (win_y = -winhalf; win_y < winhalf; win_y++)
		{
			for (win_x = -winhalf; win_x < winhalf; win_x++)
			{
				if (y + win_y >= 0 &&
					x + win_x >= 0 &&
					y + win_y < height &&
					x + win_x < width &&
					x + win_x - d >= 0 &&
					x + win_x - d < width)
				{
					//Calculate the actual ZNCC value for each windows
					standart_deviation1 = in_img_1[(win_y + y)*width + (x + win_x)] - sum_of_window_values1;
					standart_deviation2 = in_img_2[(win_y + y)*width + (x + win_x - d)] - sum_of_window_values2;
					//Variance
					var1 += standart_deviation1*standart_deviation1;
					var2 += standart_deviation2*standart_deviation2;
					multistand += standart_deviation1*standart_deviation2;
				}
				else
				{
					continue;
				}
			}
		}
		multistand /= native_sqrt(var1) * native_sqrt(var2);
		if (multistand > current_max_sum)
		{
			current_max_sum = multistand;
			best_disparity_value = d;
		}
	}
	disp_map[y*width + x] = (uint) abs(best_disparity_value);
}
