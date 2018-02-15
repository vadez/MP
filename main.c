#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lodepng.h"

#define L_INPUT_IMAGE_NAME "im0.png"
#define R_INPUT_IMAGE_NAME "im1.png"

#define MAX_DISP 65
#define BLOCK_SIZE 9
#define THRESHOLD 8
#define OUTPUT_IMAGE_NAME "depthmap.png"

void resize_rgb2gray(int w_out, int h_out, unsigned char* img_in, unsigned char* image_out)
{	//printf("Init resizeing\n");
	int img_y;
	int img_x;
	for (img_y = 0; img_y < h_out; img_y++)
	{
		for (img_x = 0; img_x < w_out; img_x++)
		{
			unsigned char Red = img_in[w_out * 64 * img_y + 16 * img_x];
			unsigned char Green = img_in[w_out * 64 * img_y + 1 + 16 * img_x];
			unsigned char Blue = img_in[w_out * 64 * img_y + 2 + 16 * img_x];
			image_out[w_out*img_y + img_x] = (unsigned char)(Red * 0.2126 + Green * 0.7152 + Blue * 0.0722);
		}
	}
}

void zncc(unsigned char* dmap, unsigned char* image1, unsigned char* image2, int height, int width, int MAXDISP, int MINDISP, int WIN_SIZE)
{

	//int Standard_deviation;
	double sum_of_window_values1;
	double sum_of_window_values2;
	double standart_deviation1;
	double standart_deviation2;
	double multistand=0;
	double best_disparity_value;
	double current_max_sum;
	double average1, average2;
	int J;
	int I;
	int d;
	int WIN_Y, WIN_X;
	double std1, std2;

	for (J = 0; J < height; J++){
		for (I = 0; I < width; I++) {
			best_disparity_value = MAXDISP;
			current_max_sum = -1;
			for (d = MINDISP; d <= MAXDISP; d++) {
				sum_of_window_values1 = 0;
				sum_of_window_values2 = 0;
				for (WIN_Y = -WIN_SIZE / 2; WIN_Y < WIN_SIZE/2; WIN_Y++ ) {
					for (WIN_X = -WIN_SIZE / 2; WIN_X < WIN_SIZE/2; WIN_X++) {

						if (J+WIN_Y>=0 && I+WIN_X>=0 && J+WIN_Y<height && I+WIN_X<width && I+WIN_X-d>=0 && I+WIN_X-d<width)
						{
							//Calculate the mean value for each window
							sum_of_window_values1 += image1[(WIN_Y + J)*width + (I + WIN_X)];
							sum_of_window_values2 += image2[(WIN_Y + J)*width + (I + WIN_X - d)];
						}
						else
							continue;


					}
				}
				average1 = sum_of_window_values1 / (WIN_SIZE*WIN_SIZE);
				average2 = sum_of_window_values2 / (WIN_SIZE*WIN_SIZE);
				std1 = 0;
				std2 = 0;
				multistand = 0;
				//window_sum = 0;
				for (WIN_Y = -WIN_SIZE / 2; WIN_Y < WIN_SIZE / 2; WIN_Y++) {
						for (WIN_X = -WIN_SIZE / 2; WIN_X < WIN_SIZE / 2; WIN_X++) {

							if (J + WIN_Y >= 0 && I + WIN_X >= 0 && J + WIN_Y < height && I + WIN_X < width && I + WIN_X - d >= 0 && I + WIN_X - d < width)
							{
								//Calculate the actual ZNCC value for each windows
								standart_deviation1 = image1[(WIN_Y + J)*width + (I + WIN_X)] - average1;
								standart_deviation2 = image2[(WIN_Y + J)*width + (I + WIN_X - d)] - average2;
								std1 += standart_deviation1*standart_deviation1;
								std2 += standart_deviation2*standart_deviation2;
								multistand += standart_deviation1*standart_deviation2;
							}
							else
								continue;

					}
				}
				multistand /= sqrt(std1) * sqrt(std2);
				if (multistand > current_max_sum) {
					current_max_sum = multistand;
					best_disparity_value=d;
				}

			}
			dmap[width*J +I] = (unsigned char)abs(best_disparity_value);
		}
	}
}

void cross_check(unsigned char* map_com, unsigned char* dispmap_L, unsigned char* dispmap_R, int w, int h, int threshold)
{
	printf("Cross check staring..\n");
	for (int i = 0; i < w*h; i++)
	{
		if (abs(dispmap_L[i] - dispmap_R[i]) > threshold)
		{
			map_com[i] = 0;
		}
		else
		{
			map_com[i] = dispmap_L[i];
		}
	}


}

void occulsion_fill(unsigned char* dispmap_OF, unsigned char* dispmap_CC, int width, int height)
{
	int win_width = 2;
	int win_height = 2;
	int sum_win;
	int pixel_count;
	int mean_win;

	for (int y_img = 0; y_img < height; y_img++)
	{
		for (int x_img = 0; x_img < width; x_img++)
		{
			if (dispmap_CC[y_img * width + x_img] == 0)
			{
				sum_win = 0;
				pixel_count = 0;

				for (int y_win = -win_height; y_win <= win_height; y_win++)
				{
					for (int x_win = -win_width; x_win <= win_width; x_win++)
					{
						if (!(y_img + y_win >= 0) ||
							!(y_img + y_win < height) ||
							!(x_img + x_win >= 0) ||
							!(x_img + x_win < width))
						{
							continue;
						}
						if (dispmap_CC[y_img * width + y_win + x_img + x_win] != 0)
						{
							sum_win += dispmap_CC[y_img * width + y_win + x_img + x_win];
							pixel_count++;
						}
					}
				}
				if (pixel_count > 0)
				{
					mean_win = sum_win / pixel_count;
					dispmap_OF[y_img * width + x_img] = mean_win;
				}
			}
			else
			{
				dispmap_OF[y_img * width + x_img] = dispmap_CC[y_img * width + x_img];
			}
		}
	}
}

void normalize_map(unsigned char* dispmap, int w, int h)
{
	printf("Starting normalizing map\n");
	int a = 0;
	int b = 255;
	int  i;
	int X_max=MAX_DISP;
	int X_min=0;
	//feature scaling
	for (i = 0; i < w*h; i++) {
		dispmap[i] = (unsigned char)(a + ((dispmap[i] - X_min)*(b - a)) / (X_max - X_min));
	}
}

int main()
{

	int error;
	//printf("Hello Wooordl\n");
	unsigned char* image_L;
	unsigned char* image_R;
	unsigned char* image_out_L;
	unsigned char* image_out_R;
	unsigned char* dmap_L;
	unsigned char* dmap_R;
	unsigned char* dmap_combined;
	unsigned char* dispmap_Of;

	unsigned w_o;
	unsigned h_o;
	unsigned w;
	unsigned h;

	error = lodepng_decode32_file(&image_L, &w, &h, L_INPUT_IMAGE_NAME);
	if (error)printf("Error detected! While decoding image\n");

	error = lodepng_decode32_file(&image_R, &w, &h, R_INPUT_IMAGE_NAME);
	if (error)printf("Error detected! While decoding image\n");

	printf("Image load maybe happened?\n");
	w_o = w / 4;
	h_o = h / 4;
	image_out_L = (unsigned char*)malloc(w_o * h_o);
	resize_rgb2gray(w_o, h_o, image_L, image_out_L);
	image_out_R = (unsigned char*)malloc(w_o * h_o);
	resize_rgb2gray(w_o, h_o, image_R, image_out_R);
	printf("Resize maybe happened?\n");
	printf("disparity calc started..\n");

	dmap_L = (unsigned char*)malloc(w_o * h_o);
	zncc(dmap_L, image_out_L, image_out_R, h_o, w_o, MAX_DISP, 0, BLOCK_SIZE);

	dmap_R = (unsigned char*)malloc(w_o * h_o);
	zncc(dmap_R, image_out_R, image_out_L, h_o, w_o, 0, -MAX_DISP, BLOCK_SIZE);

	dmap_combined = (unsigned char*)malloc(w_o * h_o);
	cross_check(dmap_combined, dmap_L, dmap_R, w_o, h_o, THRESHOLD);

	dispmap_Of = (unsigned char*)malloc(w_o * h_o);
	occulsion_fill(dispmap_Of,dmap_combined, w_o, h_o, 16);

	normalize_map(dispmap_Of, w_o, h_o);

	//error = lodepng_encode_file(imgs_out[0], image_out_L, w_o, h_o, LCT_GREY, 8);
	//error = lodepng_encode_file(imgs_out[1], image_out_R, w_o, h_o, LCT_GREY, 8);
	error = lodepng_encode_file(OUTPUT_IMAGE_NAME, dispmap_Of, w_o, h_o, LCT_GREY, 8);
	if (error)printf("Error %u: %s\n", error, lodepng_error_text(error));
	return 0;
}
