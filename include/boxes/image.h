
#ifndef BOXES_IMAGE_H
#define BOXES_IMAGE_H

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>

#include <boxes/forward_declarations.h>
#include <boxes/camera_matrix.h>
#include <boxes/constants.h>
#include <boxes/point_cloud.h>

#include <moges/Types.h>
#include <moges/NURBS/Curve.h>

namespace Boxes {
	class Image {
		public:
			Image();
			Image(const std::string filename);
			Image(cv::Mat mat);
			void init();
			~Image();

			// Basic methods.
			void show();
			void write(const std::string filename);
			cv::Size size() const;

			// mat
			const cv::Mat* get_mat() const;
			const cv::Mat* get_mat(int code) const;
			const cv::Mat* get_greyscale_mat() const;

			// descriptors
			cv::Mat* get_descriptors(std::vector<cv::KeyPoint>* keypoints,
				const std::string detector_type = DEFAULT_FEATURE_DETECTOR_EXTRACTOR) const;

			// keypoints
			std::vector<cv::KeyPoint>* get_keypoints(const std::string detector_type = DEFAULT_FEATURE_DETECTOR);

			// (good) features
			std::vector<cv::Point2f> get_good_features_to_track(int max_corners = 1500, double quality_level = 0.05,
				double min_distance = 2.0);

			// distance
			void set_distance(unsigned int distance);

			// camera matrix
			cv::Mat guess_camera_matrix() const;

			// disparity map
			Image* get_disparity_map(Image *other_img);

			// curve
			bool has_curve() const;
			std::vector<cv::Point2f> discretize_curve() const;
			void draw_curve();
			void cut_out_curve(PointCloud* point_cloud) const;

			CameraMatrix* camera_matrix = NULL;
			void update_camera_matrix(CameraMatrix* camera_matrix);

		private:
			cv::Mat mat;
			void decode_jfif_data(std::string filename);

			// curve
			MoGES::NURBS::Curve* curve = NULL;
			MoGES::NURBS::Curve* read_curve(const std::string filename) const;
			std::string find_curve_file(const std::string filename) const;

			// distance
			unsigned int distance = 0;

			// keypoint cache
			std::map<std::string, std::vector<cv::KeyPoint>*> keypoints;
			std::vector<cv::KeyPoint>* compute_keypoints(const std::string detector_type = DEFAULT_FEATURE_DETECTOR) const;
	};
};

#endif
