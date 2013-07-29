
#include <opencv2/opencv.hpp>
#include <vector>

#include <boxes/camera_matrix.h>
#include <boxes/structs.h>

namespace Boxes {
	/*
	 * Contructor.
	 */
	CameraMatrix::CameraMatrix(cv::Matx34d matrix) {
		this->matrix = matrix;
	}

	bool CameraMatrix::rotation_is_coherent() const {
		cv::Mat rotation = cv::Mat(this->matrix).colRange(0, 3);

		double determinant = fabsf(cv::determinant(rotation)) - 1.0;
		return (determinant > 1e-07);
	}

	double CameraMatrix::percentage_of_points_in_front_of_camera() const {
		unsigned int points_in_front = 0;

		for (std::vector<CloudPoint>::const_iterator cp = this->point_cloud.begin(); cp != this->point_cloud.end(); ++cp) {
			if (cp->pt.z > 0)
				points_in_front++;
		}

		return (double)points_in_front / (double)this->point_cloud.size();
	}
}