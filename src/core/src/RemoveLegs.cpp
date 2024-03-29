/**
 * @brief 
 * 
 * @file RemoveLegs.cpp
 * @author Luis Meier
 * @date 2018-07-10
 */
#include <core/RemoveLegs.h>

RemoveLegs::RemoveLegs()
{
	// Create a ROS publisher for the output point cloud
	pub = nh.advertise<sensor_msgs::PointCloud2>("/melkroboter/cloud_no_legs", 1);
	viewer = createViewer("Cloud");
	viewer->addCoordinateSystem(0.1, "original", 0);
}

void RemoveLegs::cloud_cb(const sensor_msgs::PointCloud2ConstPtr &cloud_msg)
{
	// used datasets
	pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
	sensor_msgs::PointCloud2 output;
	pcl::PointCloud<PointT>::Ptr cloud_reduced(new pcl::PointCloud<PointT>());
	pcl::PointCloud<PointT>::Ptr cloud_reduced2(new pcl::PointCloud<PointT>());

	// Convert from ROS to PCL type
	pcl::fromROSMsg(*cloud_msg, *cloud);

	viewer->removeAllPointClouds();
	viewer->removeAllShapes();
	viewer->addPointCloud(cloud, "mycloud");

	*cloud_reduced = removeLeg(cloud, 0);
	//*cloud_reduced2 = removeLeg(cloud_reduced, 1);

	viewer->addSphere(cloud->points[minPoints[0]], 0.025, "spher1");
	viewer->addSphere(cloud->points[minPoints[1]], 0.025, "sphere2");
	viewer->spinOnce();

	// Convert back to ROS data type
	pcl::toROSMsg(*cloud_reduced2, output);

	// Publish the data
	pub.publish(output);
}

// Creates, initializes and returns a new viewer.
boost::shared_ptr<pcl::visualization::PCLVisualizer>
RemoveLegs::createViewer(std::string name)
{
	boost::shared_ptr<pcl::visualization::PCLVisualizer> v(
		new pcl::visualization::PCLVisualizer(name));

	return (v);
}

pcl::PointCloud<PointT>
RemoveLegs::removeLeg(pcl::PointCloud<PointT>::Ptr &cloud, int leg)
{
	// Used Datasets

	pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
	pcl::PointCloud<PointT>::Ptr cloud_reduced(new pcl::PointCloud<PointT>);
	pcl::PointIndicesPtr cluster(new pcl::PointIndices);

	// Used Objects
	pcl::GridMinimum<PointT> minFilter(10);
	pcl::search::Search<PointT>::Ptr tree =
		boost::shared_ptr<pcl::search::Search<PointT>>(
			new pcl::search::KdTree<PointT>);
	pcl::NormalEstimation<PointT, pcl::Normal> normal_estimator;
	pcl::RegionGrowing<PointT, pcl::Normal> reg;
	pcl::ExtractIndices<PointT> extract;

	// minFilter
	minFilter.setInputCloud(cloud);
	minFilter.filter(minPoints);
	/*
    // Growing
    normal_estimator.setSearchMethod(tree);
    normal_estimator.setInputCloud(cloud);
    normal_estimator.setKSearch(50);
    normal_estimator.compute(*normals);

    reg.setMinClusterSize(50);
    reg.setMaxClusterSize(1000000);
    reg.setSearchMethod(tree);
    reg.setNumberOfNeighbours(30);
    reg.setInputCloud(cloud);
    reg.setInputNormals(normals);
    reg.setSmoothnessThreshold(6.5 / 180.0 * M_PI);
    reg.setCurvatureThreshold(6.5);

    reg.getSegmentFromPoint(minPoints[leg], *cluster);

    extract.setInputCloud(cloud);
    extract.setIndices(cluster);
    extract.setNegative(true);
    extract.filter(*cloud_reduced);
    return *cloud_reduced;*/
	return *cloud;
}