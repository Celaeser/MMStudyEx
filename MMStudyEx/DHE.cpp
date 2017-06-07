#include "DHE.h"
#include <array>
#include <cmath>

static std::pair<double, double> calcM1andLWidth(ImageMat &mat)
{
	if (mat.getType() != ImageMat::BGR) throw std::runtime_error("Input Image Type must be BGR");

	// Convert to L*a*b*
	int cols = mat.getWidth();
	int rows = mat.getHeight();
	ImageMat tempImage = ImageMat(cols, rows, 3);
	ImageMat::Byte *originData = mat.getRawData();
	ImageMat::Byte *tempData = tempImage.getRawData();

	auto f = [](double t) -> double {
		if (t > 0.008856) {
			return pow(t, 1 / 3.0);
		}
		else {
			return 7.787 * t + 16 / 116;
		}
	};

	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			ImageMat::Byte *originPix = originData + i * cols * 3 + j * 3;
			ImageMat::Byte *outputPix = tempData + i * cols * 3 + j * 3;
			ImageMat::Byte R = originPix[2];
			ImageMat::Byte G = originPix[1];
			ImageMat::Byte B = originPix[0];
			// X
			ImageMat::Byte X = (ImageMat::Byte) (0.412453 * R + 0.357580 * G + 0.180423 * B) / 0.950256;
			// Y
			ImageMat::Byte Y = (ImageMat::Byte) (0.212671 * R + 0.715160 * G + 0.072169 * B);
			// Z
			ImageMat::Byte Z = (ImageMat::Byte) (0.019334 * R + 0.119193 * G + 0.950277 * B) / 1.088754;

			ImageMat::Byte L = Y > 0.008856 ? 116 * pow(Y, 1 / 3.0) - 16 : 903.3 * Y;

			ImageMat::Byte a = 500 * (f(X) - f(Y)) + 128;

			ImageMat::Byte b = 200 * (f(Y) - f(Z)) + 128;
			L = L * 255 / 100;
			//a += 128;
			//b += 128;
			outputPix[0] = L;
			outputPix[1] = a;
			outputPix[2] = b;
		}
	}

	// Calculate M1
	// Sigma and Mu
	double sumA = 0;
	double sumB = 0;
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			sumA += tempData[i * cols * 3 + j * 3 + 1];
			sumB += tempData[i * cols * 3 + j * 3 + 2];
		}
	}
	double mu_a = sumA / (cols * rows);
	double mu_b = sumB / (cols * rows);
	double sigma_a_square = 0;
	double sigma_b_square = 0;
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			sigma_a_square += (tempData[i * cols * 3 + j * 3 + 1] - mu_a) * (tempData[i * cols * 3 + j * 3 + 1] - mu_a);
			sigma_b_square += (tempData[i * cols * 3 + j * 3 + 2] - mu_b) * (tempData[i * cols * 3 + j * 3 + 2] - mu_b);
		}
	}
	sigma_a_square = sigma_a_square / (cols * rows);
	sigma_b_square = sigma_b_square / (cols * rows);
	double sigma_ab = sqrt(sigma_a_square + sigma_b_square);

	// Calculate L width
	ImageMat::Byte min = tempData[0], max = tempData[0];
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			if (tempData[i * cols * 3 + j * 3] > max) max = tempData[i * cols * 3 + j * 3];
			if (tempData[i * cols * 3 + j * 3] < min) min = tempData[i * cols * 3 + j * 3];
		}
	}

	return std::make_pair(sigma_ab + 0.37 * (mu_a + mu_b) / 2, max - min);
}

static std::vector<uint8_t> dhe_helper(const ImageMat &origin, std::ostream& histogram, double beta)
{
	if (origin.getType() != ImageMat::HSI) throw std::runtime_error("Input Image Type must be HSI");

	std::vector<uint8_t> mapData(256, 0);
	std::array<std::pair<uint32_t, uint32_t>, 256> accuData = { std::make_pair(0, 0) };
	std::array<uint32_t, 256> countData = { 0 };
	const uint32_t TOTAL = origin.getWidth() * origin.getHeight();
	int WIDTH = origin.getWidth();
	int HEIGHT = origin.getHeight();
	const ImageMat::Byte *DATA = origin.getRawData();
	std::vector<std::vector<ImageMat::Byte>> deltaIntensity(HEIGHT);
	std::vector<std::vector<ImageMat::Byte>> deltaHue(HEIGHT);
	ImageMat::Byte f_I[256] = { 0 };
	ImageMat::Byte f_H[256] = { 0 };
	ImageMat::Byte F_I[256] = { 0 };
	ImageMat::Byte F_H[256] = { 0 };

	//Count
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			++accuData[(size_t)origin.getPixiv(i, j)[2]].first;
		}
	}

	//Record before equalization
	histogram << "before equalization : \n";
	for (int i = 0; i < 256; ++i) {
		histogram << i << " : " << accuData[i].first << '\n';
	}

	//Accumulate
	// delta intensity and hue dynamic array
	for (int i = 0; i < HEIGHT; ++i) {
		deltaIntensity[i].resize(WIDTH, 0);
		deltaHue[i].resize(WIDTH, 0);
	}

	// define some useful function
	auto H = [&](int row, int col) -> ImageMat::Byte {
		return DATA[3 * (row * WIDTH + col) + 0] / 255.0 * 360;
	};

	auto I = [&](int row, int col) -> ImageMat::Byte {
		return DATA[3 * (row * WIDTH + col) + 2];
	};

	auto T = [&](ImageMat::Byte alpha) -> ImageMat::Byte {
		ImageMat::Byte absAlpha = std::abs(alpha);
		return absAlpha <= 180 ? absAlpha : 360 - absAlpha;
	};

	// calculate delta Intensity
	for (int i = 1; i < HEIGHT - 1; ++i) {
		for (int j = 1; j < WIDTH - 1; ++j) {
			ImageMat::Byte deltaH = (I(i + 1, j + 1) + 2 * I(i + 1, j) + I(i + 1, j - 1)) -
				(I(i - 1, j + 1) + 2 * I(i - 1, j) + I(i - 1, j - 1));
			ImageMat::Byte deltaV = (I(i + 1, j + 1) + 2 * I(i, j + 1) + I(i - 1, j + 1)) -
				(I(i + 1, j - 1) + 2 * I(i, j - 1) + I(i - 1, j - 1));

			deltaIntensity[i][j] = std::sqrt(deltaH * deltaH + deltaV * deltaV);
		}
	}

	// calculate delta Hue
	for (int i = 1; i < HEIGHT - 1; ++i) {
		for (int j = 1; j < WIDTH - 1; ++j) {
			ImageMat::Byte deltaH = T(H(i + 1, j + 1) - H(i - 1, j + 1)) + 2 * T(H(i + 1, j) - H(i - 1, j)) +
				T(H(i + 1, j - 1) - H(i - 1, j - 1));
			ImageMat::Byte deltaV = T(H(i + 1, j + 1) - H(i + 1, j - 1)) + 2 * T(H(i, j + 1) - H(i, j - 1)) +
				T(H(i - 1, j + 1) - H(i - 1, j - 1));

			deltaHue[i][j] = std::sqrt(deltaH * deltaH + deltaV * deltaV);
		}
	}

	// calc the Intensity
	// f_I(k) && f_H(k)
	for (int i = 0; i < HEIGHT; ++i) {
		for (int j = 0; j < WIDTH; ++j) {
			f_I[(int)I(i, j)] += deltaIntensity[i][j];
			f_H[(int)I(i, j)] += deltaHue[i][j];
		}
	}
	// F_I(k) && F_H(k)
	F_I[0] = f_I[0];
	F_H[0] = f_H[0];
	for (int k = 1; k < 256; ++k) {
		F_I[k] = F_I[k - 1] + f_I[k];
		F_H[k] = F_H[k - 1] + f_H[k];
	}

	auto IDHE = [&](int intensity) -> ImageMat::Byte {
		return F_I[intensity] * 255.0 / F_I[255];
	};

	auto HDHE = [&](int intensity) -> ImageMat::Byte {
		return F_H[intensity] * 255.0 / F_H[255];
	};

	//Equalization
	for (int i = 1; i < 256; ++i) {
		mapData[i] = (uint8_t)std::round(beta * IDHE(i) + (1 - beta) * HDHE(i));
		countData[mapData[i]] = accuData[i].first;
	}

	//Record after equalization
	histogram << "\n\nafter equalization : \n";
	for (int i = 0; i < 256; ++i) {
		histogram << i << " : " << countData[i] << '\n';
	}

	return mapData;
}

std::vector<ImageMat> diff_histogram_equalization(const ImageMat &origin, std::ostream& histogram, double beta)
{
	std::vector<ImageMat> imageSet;
	std::vector<uint8_t> mapData;
	const uint32_t TOTAL = origin.getWidth() * origin.getHeight();

	//Get pixiv map data and output histogram
	mapData = dhe_helper(origin, histogram, beta);

	//Generate image
	imageSet.emplace_back(origin.getWidth(), origin.getHeight(), 3, origin.getType());
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			imageSet.back().getPixiv(i, j)[2] = mapData[(size_t)origin.getPixiv(i, j)[2]];
			imageSet.back().getPixiv(i, j)[0] = origin.getPixiv(i, j)[0];
			imageSet.back().getPixiv(i, j)[1] = origin.getPixiv(i, j)[1];
		}
	}

	imageSet.emplace_back(origin.getWidth(), origin.getHeight(), 1, ImageMat::Gray);
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			imageSet.back().getPixiv(i, j)[0] = origin.getPixiv(i, j)[0];
		}
	}

	imageSet.emplace_back(origin.getWidth(), origin.getHeight(), 1, ImageMat::Gray);
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			imageSet.back().getPixiv(i, j)[0] = origin.getPixiv(i, j)[1];
		}
	}

	imageSet.emplace_back(origin.getWidth(), origin.getHeight(), 1, ImageMat::Gray);
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			imageSet.back().getPixiv(i, j)[0] = origin.getPixiv(i, j)[2];
		}
	}

	imageSet.emplace_back(origin.getWidth(), origin.getHeight(), 1, ImageMat::Gray);
	for (uint32_t i = 0; i < origin.getWidth(); ++i) {
		for (uint32_t j = 0; j < origin.getHeight(); ++j) {
			imageSet.back().getPixiv(i, j)[0] = mapData[(size_t)origin.getPixiv(i, j)[2]];
		}
	}

	return imageSet;
}