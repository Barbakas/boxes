/***
	This file is part of the boxes library.

	Copyright (C) 2013-2014  Christian Bodenstein, Michael Tremer

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <boost/regex.hpp>
#include <fstream>
#include <iostream>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#ifdef BOXES_NONFREE
# include <opencv2/nonfree/features2d.hpp>
#endif

#include <boxes/boxes.h>
#include <boxes/constants.h>
#include <boxes/image.h>

#include <moges/Types.h>
#include <moges/NURBS/Curve.h>

namespace Boxes {
	Image::Image(Boxes* boxes) {
		this->init(boxes);
	}

	Image::Image(Boxes* boxes, const std::string filename, int width, int height) {
		this->filename = filename;

		this->mat = cv::imread(this->filename);
		assert(!this->mat.empty());

		// Scale the image
		if (width > 0) {
			cv::Size image_size = this->size();

			this->scaling = (double)width / (double)image_size.width;
			int calc_height = this->scaling * image_size.height;

			if ((height > 0) && (calc_height != height)) {
				std::ostringstream message;
				message << "The new resolution violates the original aspect ratio. Should be ";
				message << width << "x" << calc_height << ".";

				throw std::runtime_error(message.str());
			} else {
				height = calc_height;
			}

			cv::resize(this->mat, this->mat, cv::Size(width, height));
		}

		this->init(boxes);

		// Read available JFIF data.
		this->decode_jfif_data(this->filename);
	}

	Image::Image(Boxes* boxes, cv::Mat mat) {
		this->init(boxes);

		this->mat = mat;
	}

	void Image::init(Boxes* boxes) {
		this->boxes = boxes;

		// Initialize camera matrix
		CameraMatrix matrix(this->boxes);
		this->update_camera_matrix(&matrix);

		// Try to find a camera matrix and read it in.
		std::string filename_camera = this->find_camera_file();
		if (filename_camera.empty())
			this->camera = this->guess_camera();
		else
			this->camera = this->read_camera(filename_camera);

		// Try to find a curve and read it.
		std::string filename_curve = this->find_curve_file();
		if (!filename_curve.empty()){
			this->curve = this->read_curve(filename_curve);

			if (this->scaling > 0)
				this->curve->scale(this->scaling);
		}
	}

	Image::~Image() {
		if (this->curve)
			delete this->curve;

		// Delete all cached keypoints.
		for (std::map<const std::string, std::vector<cv::KeyPoint>*>::iterator i = this->keypoints.begin();
				i != this->keypoints.end(); i++) {
			delete i->second;
		}

		if (this->camera_matrix)
			delete this->camera_matrix;
	}

	void Image::decode_jfif_data(std::string filename) {

		// First check for distance file
		std::string distanceFileName = this->find_file_with_extension(this->filename, "distance");
		std::ifstream distanceFile(distanceFileName, std::ios::in);
		int dist = 0;
		if(distanceFile >> dist)
		{
			this->set_distance(dist);
			return;
		}	
		// If there is no distance file watch jfif header

		unsigned char header[2048];
		unsigned int pos = 0;

		std::ifstream file(filename, std::ios::in|std::ios::binary);
		if (file.is_open()) {
			file.read((char *)header, sizeof(header));
			file.close();

			// Check for magic number match.
			if(header[pos] == 0xFF && header[pos+1] == 0xD8 && header[pos+2] == 0xFF && header[pos+3] == 0xE0) {
				pos += 4;

				// Check for a valid JFIF header (JFIF\0).
				if (header[pos+2] == 'J' && header[pos+3] == 'F' && header[pos+4] == 'I' && header[pos+5] == 'F' && header[pos+6] == 0x00) {
					// Retrieve block by block...
					unsigned int block_length = (header[pos] << 8) + header[pos+1];
					while (pos < sizeof(header)) {
						pos += block_length;

						// Check if we jumped past the end of the header.
						if (pos >= sizeof(header)) {
							break;
						}

						// Handle comment block.
						if (header[pos+1] == 0xFE) {
							unsigned int comment_pos = 0;
							unsigned int comment_length = (header[pos+2] << 8) + header[pos+3] - 2;

							// Copy comment to string.
							std::string comment;
							while (comment_pos < comment_length) {
								comment += header[pos+comment_pos+4];
								comment_pos++;
							}

							// Go (for) the distance.
							/* Using boost::regex here, becase std::regex::regex_search is broken since GCC 4.7 */
							boost::regex re("Distance=\"(\\d+)\"", boost::regex_constants::extended);

							boost::smatch matches;
							if (boost::regex_search(comment, matches, re)) {
								unsigned int distance = 0;
								std::stringstream (matches[1]) >> distance;

								this->set_distance(distance);
							}
						}

						// Go to the next block.
						block_length = (header[pos+2] << 8) + header[pos+3] + 2;
					}
				}
			}
		}
	}

	void Image::show() {
		cv::namedWindow("Image", CV_WINDOW_AUTOSIZE);
		cv::imshow("Image", this->mat);
	}

	void Image::write(const std::string filename) {
		cv::imwrite(filename, this->mat);
	}

	const cv::Mat* Image::get_mat() const {
		return &this->mat;
	}

	const cv::Mat* Image::get_mat(int code) const {
		if (this->mat.type() == code) {
			return &this->mat;
		}

		cv::Mat* new_mat = new cv::Mat();
		cv::cvtColor(this->mat, *new_mat, code);

		return new_mat;
	}

	const cv::Mat* Image::get_greyscale_mat() const {
		return this->get_mat(CV_RGB2GRAY);
	}

	cv::Size Image::size() const {
		return this->mat.size();
	}

	std::vector<cv::KeyPoint>* Image::get_keypoints() {
		std::string detector_type = this->boxes->config->get("FEATURE_DETECTOR");

		return this->get_keypoints(detector_type);
	}

	std::vector<cv::KeyPoint>* Image::get_keypoints(const std::string detector_type) {
		std::vector<cv::KeyPoint>* keypoints;

		#pragma omp critical
		{
			keypoints = this->keypoints[detector_type];

			if (!keypoints) {
				keypoints = this->compute_keypoints(detector_type);
				this->keypoints[detector_type] = keypoints;
			}
		}

		return keypoints;
	}

	std::vector<cv::KeyPoint>* Image::compute_keypoints(const std::string detector_type) const {
		std::vector<cv::KeyPoint>* output = new std::vector<cv::KeyPoint>();

		cv::FeatureDetector* detector = NULL;

		// FAST
		if (detector_type == FEATURE_DETECTOR_FAST) {
			detector = new cv::FastFeatureDetector();

		// GFTT
		} else if (detector_type == FEATURE_DETECTOR_GFTT) {
			detector = new cv::GoodFeaturesToTrackDetector();

		// ORB
		} else if (detector_type == FEATURE_DETECTOR_ORB) {
			detector = new cv::OrbFeatureDetector();

		// PyradmidFast
		} else if (detector_type == FEATURE_DETECTOR_PYRAMID_FAST) {
			cv::Ptr<cv::FeatureDetector> _detector = cv::Ptr<cv::FeatureDetector>(new cv::FastFeatureDetector());

			detector = new cv::PyramidAdaptedFeatureDetector(_detector);

#ifdef BOXES_NONFREE
		// SIFT
		} else if (detector_type == FEATURE_DETECTOR_SIFT) {
			detector = new cv::SiftFeatureDetector();

		// SURF
		} else if (detector_type == FEATURE_DETECTOR_SURF) {
			int min_hessian = this->boxes->config->get_int("SURF_MIN_HESSIAN");

			detector = new cv::SurfFeatureDetector(min_hessian);
#endif
		}

		assert(detector);

		detector->detect(this->mat, *output);
		delete detector;

		return output;
	}

	cv::Mat* Image::get_descriptors(std::vector<cv::KeyPoint>* keypoints) const {
		std::string detector_type = this->boxes->config->get("FEATURE_DETECTOR_EXTRACTOR");

		return this->get_descriptors(keypoints, detector_type);
	}

	cv::Mat* Image::get_descriptors(std::vector<cv::KeyPoint>* keypoints, const std::string detector_type) const {
		cv::Mat* descriptors = new cv::Mat();

		#pragma omp critical
		{
			// Extract descriptors.
			cv::DescriptorExtractor* extractor = NULL;

			if (detector_type == FEATURE_DETECTOR_EXTRACTOR_ORB) {
				extractor = new cv::OrbDescriptorExtractor();

#ifdef BOXES_NONFREE
			} else if (detector_type == FEATURE_DETECTOR_EXTRACTOR_SIFT) {
				extractor = new cv::SiftDescriptorExtractor(48, 16, true);

			} else if (detector_type == FEATURE_DETECTOR_EXTRACTOR_SURF) {
				extractor = new cv::SurfDescriptorExtractor();
#endif
			}
			assert(extractor);

			extractor->compute(this->mat, *keypoints, *descriptors);
			delete extractor;
		}

		return descriptors;
	}

	std::vector<cv::Point2f> Image::get_good_features_to_track(int max_corners, double quality_level, double min_distance) {
		const cv::Mat* mat = this->get_greyscale_mat();

		std::vector<cv::Point2f> corners = std::vector<cv::Point2f>();

		// Find all good features to track.
		cv::goodFeaturesToTrack(*mat, corners, max_corners, quality_level, min_distance);

		// Increase the precision of the result.
		cv::cornerSubPix(*mat, corners, cv::Size(15, 15), cv::Size(-1, -1),
			cv::TermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.03));

		return corners;
	}

	void Image::set_distance(unsigned int distance) {
		this->distance = distance;
	}

	
	unsigned int Image::get_distance()
	{
		return this->distance;
	}

	cv::Mat Image::get_camera() const {
		return this->camera;
	}

	cv::Mat Image::guess_camera() const {
		cv::Size image_size = this->size();

		cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_64F);
		camera_matrix.at<double>(0, 0) = image_size.width;
		camera_matrix.at<double>(1, 1) = image_size.height;
		camera_matrix.at<double>(2, 2) = 1.0;

		// The center of the image.
		camera_matrix.at<double>(0, 2) = image_size.width / 2;
		camera_matrix.at<double>(1, 2) = image_size.height / 2;

		return camera_matrix;
	}

	cv::Mat Image::read_camera(const std::string filename) const {
		cv::Mat_<double> camera(3, 3);

		// Check if the file can be opened for reading.
		std::ifstream file(filename, std::ios::in);
		if (file.is_open()) {
			std::string element;

			for (int x = 0; x < 3; x++) {
				for (int y = 0; y < 3; y++) {
					// Read the next element from file.
					file >> element;

					// Convert to double.
					std::istringstream _element(element);
					_element >> camera.at<double>(x, y);
				}
			}

			// File exists.
			file.close();
		}

		return camera;
	}

	std::string Image::find_camera_file() const {
		return this->find_file_with_extension(this->filename, CAMERA_EXTENSION);
	}

	Image* Image::get_disparity_map(Image *other_img) {
		cv::Mat disparity_map;

		cv::StereoBM stereoBM;
		stereoBM(*this->get_greyscale_mat(), *other_img->get_greyscale_mat(), disparity_map);

		return new Image(this->boxes, disparity_map);
	}

	bool Image::has_curve() const {
		return (this->curve != NULL);
	}

	MoGES::NURBS::Curve* Image::read_curve(const std::string filename) const {
		MoGES::NURBS::Curve* curve = new MoGES::NURBS::Curve();
		curve->read(filename);

		return curve;
	}

	std::string Image::find_curve_file() const {
		return this->find_file_with_extension(this->filename, NURBS_CURVE_EXTENSION);
	}

	std::string Image::find_file_with_extension(const std::string filename, const std::string extension) const {
		std::string result;

		// End here if the original filename is unknown.
		if (filename.empty())
			return result;

		// Build a vector of candidate filenames.
		std::vector<std::string> filenames;
		filenames.push_back(filename + "." + extension);

		// Replace extension by nurbs file extension.
		std::size_t pos = filename.find_last_of(".");
		if (pos != std::string::npos) {
			std::string basename = std::string(filename.substr(0, pos));
			filenames.push_back(basename + "." + extension);
		}

		// Check for all filenames in the vector if they exist
		for (std::vector<std::string>::const_iterator i = filenames.begin(); i != filenames.end(); i++) {
			// Check if the file can be opened for reading.
			std::ifstream file(*i, std::ios::in);
			if (!file.is_open())
				continue;

			// File exists.
			file.close();

			result.assign(*i);
			break;
		}

		return result;
	}

	std::vector<cv::Point2f> Image::discretize_curve() const {
		std::vector<cv::Point2f> result;

		if (this->has_curve()) {
			MoGES::NURBS::DiscreteCurvePtr discrete_curve = this->curve->discretize();

			for (MoGES::NURBS::DiscreteCurve::iterator i = discrete_curve->begin(); i != discrete_curve->end(); i++) {
				cv::Point2f point = cv::Point2f(i->second[0], i->second[1]);
				result.push_back(point);
			}
		}

		return result;
	}

	cv::Mat Image::draw_curve() {
		cv::Mat ret(this->mat);
		std::vector<cv::Point2f> discrete_curve = this->discretize_curve();

		for (std::vector<cv::Point2f>::iterator i = discrete_curve.begin(); i != discrete_curve.end(); i++) {
			if (i->x < 0 || i->x > this->mat.cols)
				continue;

			if (i->y < 0 || i->y > this->mat.rows)
				continue;

			ret.at<cv::Vec3b>(i->y, i->x)[1] = 255;
		}
		return ret;
	}

	void Image::update_camera_matrix(CameraMatrix* camera_matrix) {
		if (this->camera_matrix != NULL)
			delete this->camera_matrix;

		this->camera_matrix = new CameraMatrix(this->boxes, camera_matrix);
	}
};
