/***
	This file is part of the boxes library.

	Copyright (C) 2013-2014  Christian Bodenstein, Michael Tremer

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <getopt.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <boxes.h>

#define VERSION_INFO \
	PACKAGE_NAME " Copyright (C) 2013-2014  Christian Bodenstein, Michael Tremer\n" \
    "This program comes with ABSOLUTELY NO WARRANTY.\n" \
    "This is free software, and you are welcome to redistribute it\n" \
    "under certain conditions.\n"

int main(int argc, char **argv) {
	Boxes::Boxes boxes;

	std::string output_depths_maps;
	std::string output_disparity_maps;
	std::string output_hull;
	std::string output_matches;
	std::string output_nurbs;
	std::string output_point_cloud;
	std::string resolution;

	bool use_optical_flow = false;
	bool visualize = false;
	bool visualize_convex_hull = false;
	bool visualize_transparent = false;

	while (1) {
		static struct option long_options[] = {
			{"algorithms",            required_argument,  0, 'a'},
			{"visualize-convex-hull", no_argument,        0, 'C'},
			{"convex-hull",           required_argument,  0, 'c'},
			{"depths-maps",           required_argument,  0, 'd'},
			{"disparity-maps",        required_argument,  0, 'D'},
			{"environment",           required_argument,  0, 'E'},
			{"environment-file",      required_argument,  0, 'e'},
			{"matches",               required_argument,  0, 'm'},
			{"nurbs",                 required_argument,  0, 'n'},
			{"optical-flow",          no_argument,        0, 'O'},
			{"point-cloud",           no_argument,        0, 'p'},
			{"resolution",            required_argument,  0, 'r'},
			{"transparent",           no_argument,        0, 't'},
			{"version",               no_argument,        0, 'V'},
			{"visualize",             no_argument,        0, 'v'},
			{0, 0, 0, 0}
		};
		int option_index = 0;

		int c = getopt_long(argc, argv, "a:Cc:D:d:E:e:m:n:Op:r:tVv", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 0:
				if (long_options[option_index].flag != 0)
					break;

				std::cerr << "option: " << long_options[option_index].name;
				if (optarg)
					std::cerr << " with argument " << optarg;
				std::cerr << std::endl;
				break;

			case 'a':
				boxes.set_algorithms(optarg);
				break;

			case 'C':
				visualize_convex_hull = true;
				break;

			case 'c':
				output_hull.assign(optarg);
				break;

			case 'D':
				output_disparity_maps.assign(optarg);
				break;

			case 'd':
				output_depths_maps.assign(optarg);
				break;

			case 'E':
				boxes.config->parse_line(optarg);
				break;

			case 'e':
				boxes.config->read(optarg);
				break;

			case 'm':
				output_matches.assign(optarg);
				break;
			case 'n':
				output_nurbs.assign(optarg);
				break;

			case 'O':
				use_optical_flow = true;
				break;

			case 'p':
				output_point_cloud.assign(optarg);
				break;

			case 'r':
				resolution.assign(optarg);
				break;

			case 't':
				visualize_transparent = true;
				break;

			case 'V':
				std::cout << VERSION_INFO << std::endl;
				std::cout << boxes.version_string() << std::endl;
				exit(0);
				break;

			case 'v':
				visualize = true;
				break;

			case '?':
				// getopt_long already printed an error message.
				break;

			default:
				abort();
		}
	}

	// Dump the configuration.
	boxes.config->dump();

	while (optind < argc) {
		std::string filename = argv[optind++];

		std::cout << "Reading image file " << filename << "..." << std::endl;
		boxes.img_read(filename, resolution);
	}

	// Warn if not enough images have been loaded.
	if (boxes.img_size() < 2) {
		std::cerr << "You need to load at least two image files! Exiting." << std::endl;
		exit(2);
	}

	Boxes::MultiCamera multi_camera(&boxes);

	// Add all images to the multi camera environment
	for (std::pair<Boxes::Image*, Boxes::Image*> image_pair: boxes.make_pairs()) {
		multi_camera.add_images(image_pair.first, image_pair.second);
	}

	multi_camera.run(use_optical_flow);

	if (!output_matches.empty()) {
		std::cout << "Writing matches..." << std::endl;
		multi_camera.write_matches_all(&output_matches);
	}
	
	if (!output_nurbs.empty()) {
		std::cout << "Writing NURBS..." << std::endl;
		multi_camera.write_nurbs_all(&output_nurbs);
	}

	if (!output_depths_maps.empty()) {
		std::cout << "Writing depths maps..." << std::endl;
		multi_camera.write_depths_map_all(&output_depths_maps);
	}

	if (!output_disparity_maps.empty()) {
		std::cout << "Writing disparity maps..." << std::endl;
		multi_camera.write_disparity_map_all(&output_disparity_maps);
	}

	Boxes::PointCloud* point_cloud = multi_camera.get_point_cloud();

	if (!output_point_cloud.empty()) {
		std::cout << "Writing point cloud to " << output_point_cloud << "..." << std::endl;
		point_cloud->write(output_point_cloud);
	}

	if (!output_hull.empty()) {
		std::cout << "Writing convex hull to " << output_hull << "..." << std::endl;
		point_cloud->write_convex_hull(output_hull);
	}

	// Print estimated volume.
	std::cout << "Estimated volume: " << point_cloud->get_volume() << std::endl;

	std::cout << "Reprojection error: " <<multi_camera.mean_reprojection_error <<std::endl;

	if (visualize)
		multi_camera.show(visualize_convex_hull, visualize_transparent);

	exit(0);
}
