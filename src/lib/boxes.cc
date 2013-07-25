
#include <list>
#include <opencv2/opencv.hpp>
#include <string>
#include <tuple>
#include <vector>

#include <boxes/boxes.h>
#include <boxes/feature_matcher.h>
#include <boxes/image.h>

namespace Boxes {

	/*
	 * Contructor.
	 */
	Boxes::Boxes() {
	}

	unsigned int Boxes::img_read(const std::string filename) {
		BoxesImage *image = new BoxesImage(filename);
		this->images.push_back(image);

		return this->images.size() - 1;
	}

	BoxesImage* Boxes::img_get(unsigned int index) {
		if (this->images.size() <= index)
			return NULL;

		std::list<BoxesImage*>::iterator i = this->images.begin();

		// Move the i pointer index steps forward.
		if (index > 0)
			std::next(i, index);

		return *i;
	}

	BoxesFeatureMatcher* Boxes::match(unsigned int index1, unsigned int index2) {
		BoxesImage* image1 = this->img_get(index1);
		assert(image1);

		BoxesImage* image2 = this->img_get(index2);
		assert(image2);

		BoxesFeatureMatcher* feature_matcher = new BoxesFeatureMatcher(image1, image2);
		return feature_matcher;

		//cv::Mat camera_matrix = this->find_best_camera_matrix(image1, image2, &matches);

		//std::cout << camera_matrix << std::endl;

		//std::cout << fundamental_matrix << std::endl;
		//std::cout << essential_matrix << std::endl;
	}

	/*
	 *
	 */
	std::string Boxes::version_string() const {
		std::string str;
		str += PACKAGE_NAME;
		str += " ";
		str += VERSION;
		str += " (";
		str += __DATE__;
		str += " ";
		str += __TIME__;
		str += ") ";

		// OpenCV
		str += "OpenCV ";
		str += CV_VERSION;

		// List active optimizations.
		std::list<std::tuple<std::string, int>> hardware_support;
		hardware_support.push_back(std::tuple<std::string, int>("MMX",      CV_CPU_MMX));
		hardware_support.push_back(std::tuple<std::string, int>("SSE",      CV_CPU_SSE));
		hardware_support.push_back(std::tuple<std::string, int>("SSE2",     CV_CPU_SSE2));
		hardware_support.push_back(std::tuple<std::string, int>("SSE3",     CV_CPU_SSE3));
		hardware_support.push_back(std::tuple<std::string, int>("SSSE3",    CV_CPU_SSSE3));
		hardware_support.push_back(std::tuple<std::string, int>("SSE4.1",   CV_CPU_SSE4_1));
		hardware_support.push_back(std::tuple<std::string, int>("SSE4.2",   CV_CPU_SSE4_2));
		hardware_support.push_back(std::tuple<std::string, int>("POPCOUNT", CV_CPU_POPCNT));
		hardware_support.push_back(std::tuple<std::string, int>("AVX",      CV_CPU_AVX));

		str += " (HW Support:";
		for (std::list<std::tuple<std::string, int>>::const_iterator i = hardware_support.begin(); i != hardware_support.end(); ++i) {
			if (cv::checkHardwareSupport(std::get<1>(*i))) {
				str += " ";
				str += std::get<0>(*i);
			}
		}
		str += ")";

		return str;
	}
}
