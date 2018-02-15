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
	//printf("W_out: %d", w_out);
	//printf("h_out: %d", h_out);
	//printf("Image: %d", &image_out);
	int img_y;
	int img_x;
	for (img_y = 0; img_y < h_out; img_y++)
	{
		for (img_x = 0; img_x < w_out; img_x++)
		{
			unsigned char Red = img_in[w_out * 64 * img_y + 16 * img_x];
			unsigned char Green = img_in[w_out * 64 * img_y + 1 + 16 * img_x];
			unsigned char Blue = img_in[w_out * 64 * img_y + 2 + 16 * img_x];
			//image_out[w_out*img_y + img_x] = img_in[w_out*4*img_y*16 + 16*img_x-3];
			image_out[w_out*img_y + img_x] = (unsigned char)(Red * 0.2126 + Green * 0.7152 + Blue * 0.0722);
		}
		//printf("%d\n", img_y);
	}
	//printf("Does return fail\n");
}


void zncc(unsigned char* dmap, unsigned char* image1, unsigned char* image2, int height, int width, int MAXDISP, int MINDISP, int WIN_SIZE)
{

	//int Standard_deviation;
	//float top_part;
	double sum_of_window_values1;
	double sum_of_window_values2;
	double standart_deviation1;
	double standart_deviation2;
	double multistand=0;
	double best_disparity_value;
	double current_max_sum;
	//double window_sum;
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
			//printf("The fock mei\n");
		}
	}
}

void cross_check(unsigned char* map_com, unsigned char* map1, unsigned char* map2, int w, int h, int threshold)
{
	printf("Cross check staring..\n");
	int idx;
	for (idx = 0; idx < w*h; idx++)
	{
		if (abs(map1[idx] - map2[idx]) > threshold)
		{
			map_com[idx] = 0;
		}
		else
		{
			map_com[idx] = map1[idx];
		}
	}


}

void occulsion_fill(unsigned char* dmap, int height, int width, int size)
{
	printf("Occulsion fill started\n");
	int x = 0;
	int y = 0;
	
	for (y = 0; y < width; y++) {
		for (x = 0; x < height; x++)
		{
			if (dmap[y*width + x] == 0) 
			{
				dmap[y*width + x] = search_non_zero(dmap,x,y,width, height,size);
			}
		}
	}

}
int search_non_zero(unsigned char* dmap, int x,int y, int width, int height, int size)
{
	for (y; y < y+ size; y++) {
		for (x; x < x+ size; x++)
		{
			if (!(y + width <= width) || !(x + height <= height))
			{
				continue;
			}
			if (dmap[y*width + x] != 0)
			{
				return dmap[y*width + x];
			}
		}
	}
	for (y; y <y- size; y--) {
		for (x; x < x- size; x--)
		{
			if (!(y+width >=0) || !(x + height >=0))
			{
				continue;
			}

			if (dmap[y*width + x] != 0)
			{
				return dmap[y*width + x];
			}
		}
	}
	return 0;
}


void normalize_map(unsigned char* nmap, int w, int h)
{
	printf("Starting normalizing map\n");
	int max = 255;
	int min = 0;
	int imsize = w*h;
	int  i;
	for (i = 0; i < imsize; i++) {
		if (nmap[i] > min)
			min = nmap[i];
		if (nmap[i] < max)
			max = nmap[i];
	}

	for (i = 0; i < imsize; i++) {
		nmap[i] = (unsigned char)(255 * (nmap[i] - max) / (min - max));
	}
}

int main()
{

	int error;
	//const char* imgs_in[2] = { "Images/img_left.png", "Images/img_right.png" };
	//const char* imgs_out[3] = { "Images/grey_left.png", "Images/grey_right.png", "Images/deathmap.png" };
	//printf("Hello Wooordl\n");
	//getchar();
	unsigned char* image_L;
	unsigned char* image_R;
	unsigned char* image_out_L;
	unsigned char* image_out_R;
	unsigned char* dmap_L;
	unsigned char* dmap_R;
	unsigned char* dmap_combined;
	unsigned char* resultmap;

	unsigned w_o;
	unsigned h_o;
	unsigned w;
	unsigned h;

	//printf("hmm\n");
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
	//zncc(dmap_R, image_out_R, image_out_L, w_o, h_o, 0, -65);

	dmap_combined = (unsigned char*)malloc(w_o * h_o);
	cross_check(dmap_combined, dmap_L, dmap_R, w_o, h_o, THRESHOLD);

	occulsion_fill(dmap_combined, w_o, h_o, 256);

	normalize_map(dmap_combined, w_o, h_o);

	//error = lodepng_encode_file(imgs_out[0], image_out_L, w_o, h_o, LCT_GREY, 8);
	//error = lodepng_encode_file(imgs_out[1], image_out_R, w_o, h_o, LCT_GREY, 8);
	error = lodepng_encode_file(OUTPUT_IMAGE_NAME, dmap_combined, w_o, h_o, LCT_GREY, 8);
	if (error)printf("Error %u: %s\n", error, lodepng_error_text(error));
	return 0;
}
