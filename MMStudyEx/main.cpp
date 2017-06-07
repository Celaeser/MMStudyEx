#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#include "Image.h"
#include "ImageProcess.h"
#include "DHE.h"
#include "getopt.h"

#include <array>

#define USAGE "=== Help Info ===\n"\
			  "-h : Show help info\n"\
			  "-i path : Set image file path\n"\
			  "-o path : Set path of output file after histogram equalization( default for outputImageX.bmp )\n"\
			  "-t path : Set path of output text file of histogram( default for histogramX.txt )\n"\
			  "-c color type(YUV \\ YCbCr \\ YIQ \\ HSI \\ HSI-DHE \\ All ) : Set color type( default for YUV )\n"

void ProcessAllTypes(const std::string& inputPath);

int main(int argc, char* argv[])
{
	::opterr = 0;
	char optch = 0;
	bool over = false;

	ImageMat::Type colorType = ImageMat::Gray;
	bool usingDHE = false;
	bool allTypes = false;
	std::string inputPath;
	std::ostringstream outpoutPath;
	std::ostringstream textPath;
	int imageCount = 0;
	std::ifstream fileTester;

	if (argc < 2) {
		std::cout << USAGE << std::endl;
		return 0;
	}

	do {
		fileTester.close();
		outpoutPath.str(std::string());
		outpoutPath << "outputImage" << imageCount << ".bmp";
		++imageCount;
		fileTester.open(outpoutPath.str());
	} while (!fileTester.fail());
	textPath << "histogram" << imageCount - 1 << ".txt";

	while ((optch = getopt(argc, argv, "hi:o:t:c:")) != -1 && !over) {
		switch (optch) {
		case 'h':
		case '?':
			std::cout << USAGE << std::endl;
			over = true;
			break;

		case 'i':
			inputPath = optarg;
			break;

		case 'o':
			outpoutPath.str(optarg);
			break;

		case 't':
			textPath.str(optarg);
			break;

		case 'c':
			if (std::strcmp(optarg, "YUV") == 0) {
				colorType = ImageMat::YUV;
			}
			else if (std::strcmp(optarg, "YCbCr") == 0) {
				colorType = ImageMat::YCbCr;
			}
			else if (std::strcmp(optarg, "YIQ") == 0) {
				colorType = ImageMat::YIQ;
			}
			else if (std::strcmp(optarg, "HSI") == 0) {
				colorType = ImageMat::HSI;
			}
			else if (std::strcmp(optarg, "HSI-DHE") == 0) {
				colorType = ImageMat::HSI; usingDHE;
				usingDHE = true;
			}
			else if (std::strcmp(optarg, "All") == 0) {
				allTypes = true;
			}
			else{
				std::cerr << "Can't do histogram equalization for type " << optarg << std::endl;
				over = true;
			}
			break;
		}
	}

	if (!over) {
		if (!allTypes) {
			std::cout << "Reading image from bmp file..." << std::endl;
			ImageMat image = ImageMat::createFromBMP(inputPath);

			std::cout << "Creating histogram text file..." << std::endl;
			std::ofstream histogramText(textPath.str());

			if (image.getRawData() == nullptr) {
				std::cout << "Can't read bmp file " << inputPath << std::endl;
				return 1;
			}

			if (image.getChannels() == 1) {
				if (colorType != ImageMat::Gray) {
					std::cerr << "Can't set color type for gray scale image." << std::endl;
					return 1;
				}
			}
			else {
				std::cout << "Color type changing..." << std::endl;
				if (colorType == ImageMat::Gray) {
					colorType = ImageMat::YUV;
				}

				image = cvtColor(image, colorType);
			}

			std::vector<ImageMat> imageSet;

			if (!usingDHE) {
				std::cout << "Histogram equalization processing..." << std::endl;
				imageSet = histogram_equalization(image, histogramText);

			}
			else {
				std::cout << "Differencial Histogram equalization processing..." << std::endl;
				imageSet = diff_histogram_equalization(image, histogramText);
			}
			std::cout << "Saving image file..." << std::endl;
			imageSet[0].saveToBMP(outpoutPath.str());

			if (image.getChannels() != 1) {
				const std::string tempPath = outpoutPath.str();
				std::string path;
				path = tempPath.substr(0, tempPath.length() - 4) + "-c0.bmp";
				imageSet[1].saveToBMP(path);
				path = tempPath.substr(0, tempPath.length() - 4) + "-c1.bmp";
				imageSet[2].saveToBMP(path);
				path = tempPath.substr(0, tempPath.length() - 4) + "-lb.bmp";
				imageSet[3].saveToBMP(path);
				path = tempPath.substr(0, tempPath.length() - 4) + "-la.bmp";
				imageSet[4].saveToBMP(path);
			}

			histogramText.close();
		}
		else {
			ProcessAllTypes(inputPath);
		}
	}

	return 0;
}

void ProcessAllTypes(const std::string& inputPath)
{
	std::cout << "Reading image from bmp file..." << std::endl;
	ImageMat image = ImageMat::createFromBMP(inputPath);

	if (image.getRawData() == nullptr) {
		std::cout << "Can't read bmp file " << inputPath << std::endl;
		std::exit(1);
	}

	std::vector<ImageMat> imageSet;
	std::ofstream histogramText;

	if (image.getChannels() == 1) {
		std::cout << "Creating histogram text file..." << std::endl;
		histogramText.open(inputPath.substr(0, inputPath.length() - 4) + "-Gray.txt");

		std::cout << "Histogram equalization processing..." << std::endl;
		imageSet = histogram_equalization(image, histogramText);

		std::cout << "Saving image file..." << std::endl;
		imageSet[0].saveToBMP(inputPath.substr(0, inputPath.length() - 4) + "-Gray.bmp");

		histogramText.close();
	}
	else {
		ImageMat::Type types[] = { ImageMat::YUV, ImageMat::YIQ, ImageMat::YCbCr, ImageMat::HSI };
		const char* bar[] = {
			"========= YUV =========", "========= YIQ =========", 
			"========= YCbCr =========", "========= HSI =========",
			"========= DHE ========="
		};
		const char* text[] = { "-YUV.txt", "-YIQ.txt", "-YCbCr.txt", "-HSI.txt", "-DHE.txt" };
		const char* (files[])[5] = { 
			{ "-YUV.bmp", "-YUV-c0.bmp", "-YUV-c1.bmp", "-YUV-lb.bmp", "-YUV-la.bmp" },
			{ "-YIQ.bmp", "-YIQ-c0.bmp", "-YIQ-c1.bmp", "-YIQ-lb.bmp", "-YIQ-la.bmp" },
			{ "-YCbCr.bmp", "-YCbCr-c0.bmp", "-YCbCr-c1.bmp", "-YCbCr-lb.bmp", "-YCbCr-la.bmp" },
			{ "-HSI.bmp", "-HSI-c0.bmp", "-HSI-c1.bmp", "-HSI-lb.bmp", "-HSI-la.bmp" },
			{ "-DHE.bmp", "-DHE-c0.bmp", "-DHE-c1.bmp", "-DHE-lb.bmp", "-DHE-la.bmp" },
		};

		auto pair = calcM1andLWidth(cvtColor(image, ImageMat::BGR));
		std::cout << "The M1 and L width value of origin is " << pair.first << " and " << pair.second << std::endl;

		for (int i = 0; i < 4; ++i) {
			std::cout << bar[i] << std::endl;
			std::cout << "Color type changing..." << std::endl;
			image = cvtColor(image, types[i]);

			std::cout << "Creating histogram text file..." << std::endl;
			histogramText.open(inputPath.substr(0, inputPath.length() - 4) + text[i]);

			std::cout << "Histogram equalization processing..." << std::endl;
			imageSet = histogram_equalization(image, histogramText);

			pair = calcM1andLWidth(cvtColor(imageSet[0], ImageMat::BGR));
			std::cout << "The M1 and L width value is " << pair.first << " and " << pair.second << std::endl;

			std::cout << "Saving image files..." << std::endl;
			for (int j = 0; j < 5; ++j) {
				imageSet[j].saveToBMP(inputPath.substr(0, inputPath.length() - 4) + files[i][j]);
			}
			histogramText.close();
		}

		std::cout << bar[4] << std::endl;
		std::cout << "Color type changing..." << std::endl;
		image = cvtColor(image, ImageMat::HSI);

		std::cout << "Creating histogram text file..." << std::endl;
		histogramText.open(inputPath.substr(0, inputPath.length() - 4) + text[4]);

		std::cout << "Differencial Histogram equalization processing..." << std::endl;
		imageSet = diff_histogram_equalization(image, histogramText);

		pair = calcM1andLWidth(cvtColor(imageSet[0], ImageMat::BGR));
		std::cout << "The M1 and L width value is " << pair.first << " and " << pair.second << std::endl;

		std::cout << "Saving image files..." << std::endl;
		for (int j = 0; j < 5; ++j) {
			imageSet[j].saveToBMP(inputPath.substr(0, inputPath.length() - 4) + files[4][j]);
		}
		histogramText.close();
	}
}