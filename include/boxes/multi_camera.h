
#ifndef BOXES_MULTI_CAMERA_H
#define BOXES_MULTI_CAMERA_H

#include <vector>

#include <boxes/feature_matcher.h>
#include <boxes/image.h>
#include <boxes/multi_camera.h>
#include <boxes/point_cloud.h>

namespace Boxes {
	class MultiCamera {
		public:
			MultiCamera();
			~MultiCamera();

			void add_images(Image* first, Image* second);
			std::pair<Image*, Image*> get_image_pair(unsigned int pair_index) const;

			void run(bool use_optical_flow);

			void write_disparity_map(unsigned int pair_index, std::string filename) const;

			PointCloud* get_point_cloud() const;

		protected:
			std::vector<std::pair<Image*, Image*>> image_pairs;
			PointCloud* point_cloud;

			FeatureMatcher* match(Image* image1, Image* image2, bool optical_flow) const;
	};
};

#endif