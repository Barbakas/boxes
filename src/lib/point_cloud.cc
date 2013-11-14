
#include <boxes/suppress_warnings.h>
INCLUDE_IGNORE_WARNINGS_BEGIN
#include <pcl/features/normal_3d.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/vtk_io.h>
#include <pcl/point_types.h>
#include <pcl/surface/gp3.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/surface/convex_hull.h>
INCLUDE_IGNORE_WARNINGS_END

#include <boxes/cloud_point.h>
#include <boxes/constants.h>
#include <boxes/converters.h>
#include <boxes/point_cloud.h>
#include <boxes/structs.h>

INCLUDE_IGNORE_WARNINGS_BEGIN
#ifdef POINT_CLOUD_USE_STATISTICAL_OUTLIER_REMOVAL
#include <pcl/filters/statistical_outlier_removal.h>
#endif
INCLUDE_IGNORE_WARNINGS_END

namespace Boxes {
	/*
	 * Contructor.
	 */
	PointCloud::PointCloud() {
	}

	PointCloud::~PointCloud() {
		this->reset_convex_hull();
	}

	unsigned int PointCloud::size() const {
		return this->points.size();
	}

	void PointCloud::add_point(CloudPoint point) {
		this->points.push_back(point);
	}

	void PointCloud::remove_point(const CloudPoint* point) {
		int i = 0;

		for (CloudPoint cp: this->points) {
			if (&cp == point) {
				this->points.erase(this->points.begin() + i);
				break;
			}

			i++;
		}
	}

	const std::vector<CloudPoint>* PointCloud::get_points() const {
		return &this->points;
	}

	std::vector<CloudPoint>::iterator PointCloud::begin() {
		return this->points.begin();
	}

	std::vector<CloudPoint>::const_iterator PointCloud::begin() const {
		return this->points.begin();
	}

	std::vector<CloudPoint>::iterator PointCloud::end() {
		return this->points.end();
	}

	std::vector<CloudPoint>::const_iterator PointCloud::end() const {
		return this->points.end();
	}

	void PointCloud::merge(const PointCloud* other) {
		for (std::vector<CloudPoint>::const_iterator i = other->begin(); i != other->end(); i++) {
			this->add_point(*i);
		}
	}

	void PointCloud::write(const std::string filename) const {
		pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud = this->generate_pcl_point_cloud();

		pcl::io::savePCDFileASCII(filename, *point_cloud);
	}

	pcl::PolygonMesh* PointCloud::triangulate() const {
		pcl::PolygonMesh* triangles = new pcl::PolygonMesh();

		if (this->size() > 0) {
			// Concert point cloud into PCL format.
			pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_rgb = this->generate_pcl_point_cloud();
			pcl::PointCloud<pcl::PointXYZ>::Ptr cloud = convertPointCloud(cloud_rgb);

			// Normal estimation
			pcl::PointCloud<pcl::Normal>::Ptr normals = this->estimate_normals(cloud);

			// Concatenate points and normal fields.
			pcl::PointCloud<pcl::PointNormal>::Ptr cloud_with_normals(new pcl::PointCloud<pcl::PointNormal>);
			pcl::concatenateFields(*cloud, *normals, *cloud_with_normals);

			// Create search tree
			pcl::search::KdTree<pcl::PointNormal>::Ptr tree(new pcl::search::KdTree<pcl::PointNormal>);
			tree->setInputCloud(cloud_with_normals);

			// Initialize triangulation
			pcl::GreedyProjectionTriangulation<pcl::PointNormal> pt = pcl::GreedyProjectionTriangulation<pcl::PointNormal>();

			// Set the maximum distance between connected points (maximum edge length)
			pt.setSearchRadius(POINT_CLOUD_TRIANGULATION_SEARCH_RADIUS);

			// Set typical values for the parameters
			pt.setMu(POINT_CLOUD_TRIANGULATION_MULTIPLIER);
			pt.setMaximumNearestNeighbors(POINT_CLOUD_TRIANGULATION_MAX_NEAREST_NEIGHBOUR);
			pt.setMaximumSurfaceAngle(POINT_CLOUD_TRIANGULATION_MAX_SURFACE_ANGLE);
			pt.setMinimumAngle(POINT_CLOUD_TRIANGULATION_MIN_ANGLE);
			pt.setMaximumAngle(POINT_CLOUD_TRIANGULATION_MAX_ANGLE);
			pt.setNormalConsistency(false);

			// Get result
			pt.setInputCloud(cloud_with_normals);
			pt.setSearchMethod(tree);
			pt.reconstruct(*triangles);
		}

		return triangles;
	}

	void PointCloud::write_polygon_mesh(std::string filename, pcl::PolygonMesh* mesh) const {
		pcl::io::saveVTKFile(filename, *mesh);
	}

	pcl::PointCloud<pcl::Normal>::Ptr PointCloud::estimate_normals(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) const {
		pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);

		if (cloud->size() > 0) {
			pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
			tree->setInputCloud(cloud);

			pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation;
			normal_estimation.setInputCloud(cloud);
			normal_estimation.setSearchMethod(tree);
			normal_estimation.setKSearch(20);
			normal_estimation.compute(*normals);
		}

		return normals;
	}

	pcl::PointCloud<pcl::PointXYZRGB>::Ptr PointCloud::generate_pcl_point_cloud() const {
		pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

		for (std::vector<CloudPoint>::const_iterator i = this->begin(); i != this->end(); ++i) {
			pcl::PointXYZRGB cloud_point;
			cloud_point.x = i->pt.x;
			cloud_point.y = i->pt.y;
			cloud_point.z = i->pt.z;

			// Apply RGB value from image for this pixel.
			// Otherwise set them in a bright colour.
			cloud_point.rgb = i->get_colour(0xffffff);

			cloud->push_back(cloud_point);
		}

		cloud->width = (uint32_t) cloud->points.size();
		cloud->height = 1;

#ifdef POINT_CLOUD_USE_STATISTICAL_OUTLIER_REMOVAL
		if (cloud->points.size() > 0) {
			pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZRGB>);

			pcl::StatisticalOutlierRemoval<pcl::PointXYZRGB> sor;
			sor.setInputCloud(cloud);
			sor.setMeanK(50);
			sor.setStddevMulThresh(1.0);
			sor.filter(*cloud_filtered);

			return cloud_filtered;
		}
#endif

		return cloud;
	}

	void PointCloud::write_depths_map(std::string filename, Image* image) const {
		double val_min, val_max;
		std::vector<double> depths(this->points.size());
		for (std::vector<CloudPoint>::const_iterator i = this->begin(); i != this->end(); ++i) {
			depths.push_back(i->pt.z);
		}
		cv::minMaxLoc(depths, &val_min, &val_max);

		cv::Mat map;
		cvtColor(*image->get_mat(), map, CV_BGR2HSV);

		for (std::vector<CloudPoint>::const_iterator i = this->begin(); i != this->end(); ++i) {
			double d = MAX(MIN((i->pt.z - val_min) / (val_max - val_min), 1.0), 0.0);
			cv::circle(map, i->keypoint.pt, 1, cv::Scalar(255.0 * (1.0 - d), 255, 255), CV_FILLED);
		}
		cvtColor(map, map, CV_HSV2BGR);

		Image image_map = Image(map);
		image_map.write(filename);
	}

	void PointCloud::show() const {
		pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud = this->generate_pcl_point_cloud();

		// Visualize.
		pcl::visualization::CloudViewer viewer("3D Point Cloud");
		viewer.showCloud(cloud, "cloud");

		while (!viewer.wasStopped()) {}
	}

	const pcl::ConvexHull<pcl::PointXYZRGB>* PointCloud::get_convex_hull() {
		#pragma omp critical
		{
			if (!this->convex_hull) {
				this->convex_hull = new pcl::ConvexHull<pcl::PointXYZRGB>();
				this->convex_hull_mesh = new pcl::PolygonMesh();

				this->compute_convex_hull(this->convex_hull, this->convex_hull_mesh);
			}
		}

		return this->convex_hull;
	}

	const pcl::PolygonMesh* PointCloud::get_convex_hull_mesh() {
		// Calling this makes sure that there is some data to return.
		this->get_convex_hull();

		return this->convex_hull_mesh;
	}

	void PointCloud::compute_convex_hull(pcl::ConvexHull<pcl::PointXYZRGB>* convex_hull, pcl::PolygonMesh* convex_hull_mesh) const {
		// Enable computation of area and volume.
		convex_hull->setComputeAreaVolume(true);

		// Make this a three-dimensional object
		convex_hull->setDimension(3);

		// Create convex hull
		pcl::PointCloud<pcl::PointXYZRGB>::Ptr pcl_point_cloud = generate_pcl_point_cloud();
		convex_hull->setInputCloud(pcl_point_cloud);
		convex_hull->reconstruct(*convex_hull_mesh);
	}

	void PointCloud::reset_convex_hull() {
		if (!this->convex_hull)
			return;

		#pragma omp critical
		{
			delete this->convex_hull;
			this->convex_hull = NULL;
		}
	}

	void PointCloud::write_convex_hull(const std::string filename) {
		const pcl::PolygonMesh* convex_hull_mesh = this->get_convex_hull_mesh();

		pcl::io::saveVTKFile(filename, *convex_hull_mesh);
	}

	double PointCloud::get_volume() {
		const pcl::ConvexHull<pcl::PointXYZRGB>* convex_hull = this->get_convex_hull();

		return convex_hull->getTotalVolume();
	}
}
