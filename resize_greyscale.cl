__kernel void resize_greyscale(__read_only image2d_t in_img_1,
	__read_only image2d_t in_img_2,
	__global uchar *out_img_1,
	__global uchar *out_img_2,
	sampler_t sampler,
	int width,
	int height)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);

	// Sample original image
	int2 coords = { 4 * x, 4 * y };
	uint4 pxl_1 = read_imageui(in_img_1, sampler, coords);
	uint4 pxl_2 = read_imageui(in_img_2, sampler, coords);

	// Write resized and greyscale image to buffer
	out_img_1[y*width + x] = 0.2126*pxl_1.x + 0.7152*pxl_1.y + 0.0722*pxl_1.z;
	out_img_2[y*width + x] = 0.2126*pxl_2.x + 0.7152*pxl_2.y + 0.0722*pxl_2.z;
}
