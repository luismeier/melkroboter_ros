#include <core/SearchTeat.h>

SearchTeat::SearchTeat(ros::NodeHandle *nodehandle, float gridSize,
					   float teatDiameter, float teatLength)
	: nh_(*nodehandle)
{
	gridSize_ = gridSize;
	teatDiameter_ = teatDiameter;
	teatLength_ = teatLength;

	initializePublishers();
	initializeSubscribers();
	initializeServices();
	cloud_.reset(new pcl::PointCloud<PointT>());

#ifdef enable_visualizer_
	viewer = createViewer("Cloud");
	viewer->addCoordinateSystem(0.1, "original", 0);
#endif
}

void SearchTeat::initializeSubscribers()
{
	ROS_INFO("Initializing Subscribers");
	sub_ = nh_.subscribe("/cloud/RotatedFiltered", 1,
						 &SearchTeat::SearchTeat::cloud_cb_, this);
}

void SearchTeat::initializePublishers()
{
	ROS_INFO("Initializing Publishers");
	// Create a ROS publisher for the output point cloud
	pub_teat_vl =
		nh_.advertise<geometry_msgs::PointStamped>("/melkroboter/teats/vl", 1);
	pub_teat_vr =
		nh_.advertise<geometry_msgs::PointStamped>("/melkroboter/teats/vr", 1);
	pub_teat_hl =
		nh_.advertise<geometry_msgs::PointStamped>("/melkroboter/teats/hl", 1);
	pub_teat_hr =
		nh_.advertise<geometry_msgs::PointStamped>("/melkroboter/teats/hr", 1);
	pub_teat_counter =
		nh_.advertise<core::teatCount>("/melkroboter/teats/counter", 1);
}

void SearchTeat::initializeServices()
{
	ROS_INFO("Initializing Services");
	searchTeat_service_ = nh_.advertiseService(
		"/Melkroboter/SearchTeat", &SearchTeat::SearchTeat::Service_cb_, this);
}

bool SearchTeat::Service_cb_(core::TeatSearchService::Request &req,
							 core::TeatSearchService::Response &res)
{
	ROS_INFO("Service CB");
	maxTeatCount = 0;
	return true;
}

void SearchTeat::cloud_cb_(const sensor_msgs::PointCloud2ConstPtr &cloud_msg)
{
	// Convert from ROS to PCL type
	cloud_.reset(new pcl::PointCloud<PointT>());
	pcl::fromROSMsg(*cloud_msg, *cloud_);
}

void SearchTeat::Searchloop()
{
	double startTime = ros::Time::now().toSec();
	// used datasets
	std::vector<int> teatCandidates;
	pcl::PointCloud<PointT>::Ptr cloud_original(new pcl::PointCloud<PointT>());
	pcl::PointCloud<PointT>::Ptr cloud_freezed(new pcl::PointCloud<PointT>());

	// Counter for the teats and vectors to store the coordinates
	int teatCount_new = 0;
	std::vector<double> xVector_new;
	std::vector<double> yVector_new;
	std::vector<double> zVector_new;

	// Freeze the newest cloud to process it
	pcl::copyPointCloud(*cloud_, *cloud_freezed);
	teatCandidates = getMinPoints(cloud_freezed);

	// goes through all possible teat candidates if one is a teat the
	// coordinates of the tip get pushed back into the vectors
	// and the teatCount gets incremented
	for (size_t i = 0; i < teatCandidates.size(); i++)
	{
		if (isTeat(teatCandidates[i], cloud_freezed))
		{
			teatCount_new++;
			xVector_new.push_back((cloud_freezed->points[teatCandidates[i]].x));
			yVector_new.push_back((cloud_freezed->points[teatCandidates[i]].y));
			zVector_new.push_back(cloud_freezed->points[teatCandidates[i]].z);
		}
	}
	// Replace the old teat vectors with the new ones
	xVector = xVector_new;
	yVector = yVector_new;
	zVector = zVector_new;
	// Replace the counter for the teats
	teatCount = teatCount_new;

	double duration = ros::Time::now().toSec() - startTime;
	ROS_INFO("Found %i Teats in %f Seconds", teatCount, duration);

	// Publish the teats only if more than 3 found
	if (teatCount > 2)
		publishTeats(cloud_freezed);

#ifdef enable_visualizer_
	updateCloud(teatCandidates);
#endif
}

void SearchTeat::publishTeats(pcl::PointCloud<PointT>::Ptr &cloud)
{
	// Calculate the middle point between all teats
	PointT midPoint;
	for (size_t i = 0; i < teatCount; i++)
	{
		midPoint.x += xVector[i];
		midPoint.y += yVector[i];
	}
	midPoint.x = midPoint.x / teatCount;
	midPoint.y = midPoint.y / teatCount;

	geometry_msgs::PointStamped pointStamped;

	for (size_t i = 0; i < teatCount; i++)
	{
		pointStamped.header.frame_id = (*cloud).header.frame_id;
		pointStamped.header.stamp = pcl_conversions::fromPCL(
			(*cloud).header.stamp); // PCL header to ROS header
		pointStamped.point.x = xVector[i];
		pointStamped.point.y = yVector[i];
		pointStamped.point.z = zVector[i];
		if (xVector[i] < midPoint.x) // Front teats
		{
			if (yVector[i] < midPoint.y)
			{
				pub_teat_vl.publish(pointStamped);
			}
			else
			{
				pub_teat_vr.publish(pointStamped);
			}
		}
		else // Rear teats
		{
			if (yVector[i] < midPoint.y)
			{
				pub_teat_hl.publish(pointStamped);
			}
			else
			{
				pub_teat_hr.publish(pointStamped);
			}
		}
	}

	// Publish the teatcount and maxTeatCount
	if (teatCount > maxTeatCount)
		maxTeatCount = teatCount;
	core::teatCount msg;
	msg.count = teatCount;
	msg.maxCount = maxTeatCount;
	pub_teat_counter.publish(msg);
}

std::vector<int> SearchTeat::getMinPoints(pcl::PointCloud<PointT>::Ptr &cloud)
{
	std::vector<int> teatCandidates_;
	double startTime = ros::Time::now().toSec();
	pcl::GridMinimum<PointT> gridMin(gridSize_);
	gridMin.setInputCloud(cloud);
	gridMin.filter(teatCandidates_);
	double endTime = ros::Time::now().toSec();
	double usedTime = endTime - startTime;
	ROS_INFO("Found %lu Candidates in %f Seconds", teatCandidates_.size(),
			 usedTime);
	return teatCandidates_;
}

#ifdef enable_visualizer_
// Creates, initializes and returns a new viewer.
boost::shared_ptr<pcl::visualization::PCLVisualizer>
SearchTeat::createViewer(std::string name)
{
	boost::shared_ptr<pcl::visualization::PCLVisualizer> v(
		new pcl::visualization::PCLVisualizer(name));
	return (v);
}

void SearchTeat::updateCloud(std::vector<int> teatCandidates)
{
	viewer->removeAllShapes();
	viewer->removeAllPointClouds();
	viewer->addPointCloud(cloud_, "cloud");
	for (size_t i = 0; i < teatCandidates.size(); i++)
	{
		std::stringstream stream;
		stream << "teatnrs " << i;
		std::string candidateNr = stream.str();

		viewer->addSphere(cloud_->points[teatCandidates[i]], 0.008, 255, 0, 0,
						  candidateNr);
		viewer->spinOnce();
	}
}
#endif

bool SearchTeat::isTeat(int indexPoint, pcl::PointCloud<PointT>::Ptr &cloud)
{
	// Objects
	pcl::KdTreeFLANN<PointT> kdtree;

	// Datasets
	bool isteat = false;

	float teatRadius = teatDiameter_ / 2000; // to get radius in meter
	float teatLength = teatLength_ / 1000;   // to get meter
	float teatLengthSq = teatLength * teatLength;
	float teatRadiusSq = teatRadius * teatRadius;
	float dot, dSq;
	Vec3 teatAxisVector;
	Vec3 searchPVec;
	PointT startP, searchP, endP;

	// Search for nearest Neighbour
	int K = 1000; // number of neighours
	std::vector<int> pointIdxSearch(K);
	std::vector<float> pointIdxSqDistance(K);
	kdtree.setInputCloud(cloud);
	// Init teataxis
	teatAxisVector.x = 0.0f;
	teatAxisVector.y = 0.0f;
	teatAxisVector.z = teatLength;
	// Init startPoint (lowes teat point)
	startP.x = cloud->points[indexPoint].x;
	startP.y = cloud->points[indexPoint].y;
	startP.z = cloud->points[indexPoint].z;
	// Init Endpoint
	endP.x = startP.x + teatAxisVector.x;
	endP.y = startP.y + teatAxisVector.y;
	endP.z = startP.z + teatAxisVector.z;

	// Search for neigbhour points
	if (kdtree.nearestKSearch(startP, K, pointIdxSearch, pointIdxSqDistance) >
		0)
	{
		for (size_t i = 0; i < pointIdxSearch.size(); i++)
		{
			// Init searchpoint
			searchP.x = cloud->points[pointIdxSearch[i]].x;
			searchP.y = cloud->points[pointIdxSearch[i]].y;
			searchP.z = cloud->points[pointIdxSearch[i]].z;

			// Vector startpoint to searchpoint
			searchPVec.x = searchP.x - startP.x;
			searchPVec.y = searchP.y - startP.y;
			searchPVec.z = searchP.z - startP.z;

			// Dot Product between
			dot = (searchPVec.x * teatAxisVector.x) +
				  (searchPVec.y * teatAxisVector.y) +
				  (searchPVec.z * teatAxisVector.z);

			// if dot product is larger than zero and smaller than the squared
			// teatLength the point is between the lowest teat point and the end
			// of
			// the defined cylinder
			if (dot < 0 || dot > teatLengthSq)
			{
				// Point is not insed of start & end bounds
				isteat = false;
				return isteat;
			}
			else
			{
				// Point is inside of Bounds
				dSq =
					(searchPVec.x * searchPVec.x + searchPVec.y * searchPVec.y +
					 searchPVec.z * searchPVec.z) -
					(dot * dot) / teatLengthSq;
				if (dSq > teatRadiusSq)
				{
					isteat = false;
					return isteat;
				}
			}
		}
		isteat = true;
		return isteat;
	}
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "SearchTeat_Node");
	ros::NodeHandle nh;
	/*ros::AsyncSpinner spinner(4);
	spinner.start();*/

	ros::Duration(5).sleep(); // sleep for half a second
	SearchTeat searchTeat(&nh, 0.08, 30.0, 50.0);

	ros::Rate loop_rate(30); // Freq in Hz
	ROS_INFO("SearchTeatNode is up");
	while (ros::ok())
	{
		ros::spinOnce();
		searchTeat.Searchloop();
		loop_rate.sleep();
	}

	return 0;
}