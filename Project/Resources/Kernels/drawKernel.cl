__constant int imageSize = 250;

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

void drawRectFunc(global unsigned int *result, int posx, int posy, int w, float3 RGB, float alpha) {
	int offset = get_group_id(0) * (3 * imageSize * imageSize);
	int start = (posy*imageSize*3)+(posx*3);

	for(int y=-w; y<w; y++) {
		for(int x=-w; x<w; x++) {
			if(posy+y > 0 && posy+y < imageSize
			&& posx+x > 0 && posx+x < imageSize) { // X and Y out of bounds checks
				//////////////////////////////
				//TODO: THESE NEED TO BE ATOMIC OPERATIONS
				//////////////////////////////
				result[offset+start+(y*imageSize*3)+(x*3)+0] = (result[offset+start+(y*imageSize*3)+(x*3)+0]*(1-alpha))+RGB.x; // Red
				result[offset+start+(y*imageSize*3)+(x*3)+1] = (result[offset+start+(y*imageSize*3)+(x*3)+1]*(1-alpha))+RGB.y; // Green
				result[offset+start+(y*imageSize*3)+(x*3)+2] = (result[offset+start+(y*imageSize*3)+(x*3)+2]*(1-alpha))+RGB.z; // Blue
				//atomic_xchg(&result[offset+start+(y*imageSize*3)+(x*3)+0], RGB.x);
				//atomic_xchg(&result[offset+start+(y*imageSize*3)+(x*3)+1], RGB.y);
				//atomic_xchg(&result[offset+start+(y*imageSize*3)+(x*3)+2], RGB.z);
			}
		}
	}
}

kernel void drawRect(global const unsigned int *vals,
					 global unsigned int *result) {
	int gid = get_global_id(0);

	float3 RGB;
	int start = gid * 7;
	float alpha    = (vals[start + 6] % 255)/255.0f;

	int posx = vals[start + 0] % 255;
	int posy = vals[start + 1] % 255;
	int w	 = (vals[start + 2] % 40);
	RGB.x    = vals[start + 3] % (int)(255 * alpha);
	RGB.y    = vals[start + 4] % (int)(255 * alpha);
	RGB.z    = vals[start + 5] % (int)(255 * alpha);

	drawRectFunc(result, posx, posy, w, RGB, alpha);
}

kernel void calcFitness(global const unsigned int *srcImg,
						global const unsigned int *resImg,
						global unsigned int *fitness) {
	long int gid = get_global_id(0)*3;

	unsigned char srcRed = srcImg[(gid % (3 * imageSize * imageSize)) + 0];
	unsigned char srcGreen = srcImg[(gid % (3 * imageSize * imageSize)) + 1];
	unsigned char srcBlue = srcImg[(gid % (3 * imageSize * imageSize)) + 2];
	unsigned char resRed = resImg[gid + 0];
	unsigned char resGreen = resImg[gid + 1];
	unsigned char resBlue = resImg[gid + 2];

	int diff = abs(srcRed-resRed) + abs(srcGreen-resGreen) + abs(srcBlue-resBlue);
	atomic_add(&fitness[get_global_id(0) / (imageSize * imageSize)], diff);
}