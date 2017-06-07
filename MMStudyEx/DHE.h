#pragma once
#include <vector>
#include "Image.h"

/**
*
* @param origin             The Origin Image
* @param histogram			The histogram text output stream( include the original data and the data after histogram equalization )
* @param beta				The parameter of combination of IDHE and HDHE
* @return					As a gray scale image, there are one image :
*								-The image after histogram equalization
*							As a colorful image, there are four images( by order ) :
*								-The image after differencial histogram equalization in luminance
*								-The chrominance image 1( U for YUV, Cb for YCbCr, H for HSI )
*								-The chrominance image 2( V for YUV, Cr for YCbCr, S for HSI )
*								-The original luminance image
*								-The luminance image after histogram equalization
*/
std::vector<ImageMat> diff_histogram_equalization(const ImageMat &origin, std::ostream& histogram, double beta = 0.6);