/**
 * @file capture_example.hh
 * 
 * @brief Example application using object spawner and camera plugins
 * 
 * This example spawns randomly generated objects and captures images from 
 * a camera in a given position.
 * 
 * It requires that
 * - gazebo-utils/media folder is adequatly populated with different materials
 * - gazebo-utils/model folder has the custom_camera.sdf, custom_sun.sdf and custom_ground.sdf models
 * - output_dir exists 
 *
 * @param[in]  _argc  The number of command-line arguments
 * @param      _argv  The argv The value of the command-line arguments
 *
 * @return     0
 */

#include "capture_example.hh"
#include <Eigen/Dense>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

double dRand(double fMin, double fMax)
{
    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    double f = (double) dist(mt) / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

/** Protect access to object_count */
std::mutex object_count_mutex;
int object_count{0};
/** Protect access to camera success */
std::mutex camera_success_mutex;
int camera_success{0};


BoundingBox3d bbs_3d;
BoundingBox2d points_2d;

std::vector<Object> objects;
std::vector<std::string> object_classes;
std::vector<int> cells_array;

int box_counter=0;
int cylinder_counter=0;
int sphere_counter=0;

const double x_size=5.0;
const double y_size=5.0;

const unsigned int x_cells = 5;
const unsigned int y_cells = 5;

double grid_cell_size_x = x_size/x_cells;
double grid_cell_size_y = y_size/y_cells;

int min_objects = 5;
int max_objects = 10;


std::shared_ptr<CameraInfo> camera_info;
int main(int argc, char **argv)
{

    /* TODO - Process options */
    if (argc < 4)
    {
        std::cout << "invalid number of arguments"<< std::endl;
        exit(-1);
    }

    std::string media_dir = std::string(argv[1]);
    unsigned int scenes = atoi(argv[2]);
    std::string dataset_dir = std::string(argv[3]);
	
    /* Create folder for storing training data */
    boost::filesystem::path traindir(dataset_dir);
    if(boost::filesystem::create_directory(traindir)) {
	std::cout << "Successfuly created train directory:" << traindir << std::endl;
    }


    std::string materials_dir   = media_dir + "/materials";
    std::string scripts_dir     = media_dir + "/materials/scripts";

    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    /* Load gazebo as a client */
    #if GAZEBO_MAJOR_VERSION < 6
    gazebo::setupClient(argc, argv);
    #else
    gazebo::client::setup(argc, argv);
    #endif

    /* Create the communication node */
    gazebo::transport::NodePtr node(new gazebo::transport::Node());
    node->Init();

    /* Publish to the object spawner request topic */
    gazebo::transport::PublisherPtr pub_world =
        node->Advertise<world_utils::msgs::WorldUtilsRequest>(WORLD_UTILS_TOPIC);

    /* Subscribe to the object spawner reply topic and link callback function */
    gazebo::transport::SubscriberPtr sub_world =
        node->Subscribe(WORLD_UTILS_RESPONSE_TOPIC, onWorldUtilsResponse);

    /* Publish to the camera topic */
    gazebo::transport::PublisherPtr pub_camera =
        node->Advertise<camera_utils::msgs::CameraUtilsRequest>(CAMERA_UTILS_TOPIC);

     /* Subscribe to the camera utils reply topic and link callback function */
    gazebo::transport::SubscriberPtr sub_camera =
        node->Subscribe(CAMERA_UTILS_RESPONSE_TOPIC, onCameraUtilsResponse);

    /* Wait for a subscriber to connect */
    pub_world->WaitForConnection();

    /* Create a vector with the name of every texture in the textures dir */
    std::vector<std::string> textures;
    for (auto &p : fs::directory_iterator(scripts_dir)){
        std::string aux(fs::basename(p));
        textures.push_back(aux.c_str());
    }

    /* Auxiliary variables */
    ignition::math::Pose3d camera_pose;
    ignition::math::Vector3d camera_position(0, 0, 5.0);
    camera_pose=getRandomCameraPose(camera_position);

    /* Ensure no objects are spawned on the server */
    std::cout <<"clean" << std::endl;
    clearWorld(pub_world);
    //clearWorld(pub_world,"plugin");
    while (waitForSpawner(0)){
        usleep(10000);
        queryModelCount(pub_world);
    }
    usleep(100000);

    world_utils::msgs::WorldUtilsRequest msg_basic_objects;
    msg_basic_objects.set_type(SPAWN);
    spawnModelFromFile(msg_basic_objects, "models/custom_sun.sdf", true, false, false, textures);
    spawnModelFromFile(msg_basic_objects, "models/custom_camera.sdf", false, true, false, textures,  camera_pose.Pos(), camera_pose.Rot());
    pub_world->Publish(msg_basic_objects);

    pub_camera->WaitForConnection();
    
    while (waitForSpawner(1)){
      usleep(10000);
      queryModelCount(pub_world);
    }

    /* Create cell grid */

    for (int i = 0;  i < x_cells * y_cells; ++i){
      cells_array.push_back(i);
    }
    
    /* Disable physics */
    changePhysics(pub_world, false);

    std::cout << "Query camera parameters" << std::endl; 
    queryCameraParameters(pub_camera);
    while (waitForCamera()){
       usleep(10000);
    }
    std::cout << "Done" << std::endl; 
    /* Main loop */
    for (int i = 468; i < scenes; i++){


        /* Random object number */
        int num_objects = (dist(mt) % max_objects) + min_objects;

        /* DEBUG */
        std::cout << "Scene " << i << " - Number of objects:" << num_objects << std::endl;
        
	/*while (pub_world->GetOutgoingCount()>0){
		usleep(10000);
	}*/

        /* Spawn ground + random objects */
	std::cout << "spawn objects" << std::endl;
        world_utils::msgs::WorldUtilsRequest msg_random_objects;
        msg_random_objects.set_type(SPAWN);
        spawnModelFromFile(msg_random_objects, "models/custom_ground.sdf", false, false, true, textures);
	objects.clear();
	spawnRandomObject(msg_random_objects, textures, grid_cell_size_x, grid_cell_size_y, num_objects, objects);
        pub_world->Publish(msg_random_objects);
        
	while (waitForSpawner(num_objects + 2)){
            usleep(10000);
            queryModelCount(pub_world);
        }

	while (pub_world->GetOutgoingCount()>0){
	    usleep(10000);
	}
	sleep(1.0);
	std::cout << "done" << std::endl; 

        /* Capture the scene and save it to a file */
	std::cout << "capture scene" << std::endl;
        captureScene(pub_camera, i);
        while (waitForCamera()){

            usleep(10000);
        }
	std::cout << "done" << std::endl;

        /* Get 3d bounding boxes */
	std::cout << "getting 3d bounding boxes..." << std::endl;
        bbs_3d.clear();
	queryModelBoundingBox(pub_world, objects);
	if(bbs_3d.size()!=num_objects)
	{
		while (bbs_3d.size()!=num_objects){
		    usleep(10000);
		}
	}
	std::cout << "done" << std::endl;

	/* Get 2D image points given 3d bounding box (8 3d points) */
	std::cout << "getting 2d bounding boxes..." << std::endl;
	points_2d.clear();
	query2DcameraPoint(pub_camera,objects);
	if(points_2d.size()!=8*num_objects)
	{
		while (points_2d.size()<num_objects){
		    usleep(10000);
		}
	}
	std::cout << "done" << std::endl;

	/* Get bounding boxes */
	std::vector<cv::Rect> boundRect( num_objects );

	for(int j=0;j<num_objects;++j)
	{
		std::pair <BoundingBox2d::iterator, BoundingBox2d::iterator> ret;
		ret = points_2d.equal_range(objects[j].name);
		std::vector<cv::Point> contours_poly( 8 );
		int p=0;

		for (std::multimap<std::string,ignition::math::Vector2d>::iterator it=ret.first; it!=ret.second; ++it)
		{	
			contours_poly[p++]=cv::Point(it->second.X(),it->second.Y());
		}

       		boundRect[j]=cv::boundingRect( cv::Mat(contours_poly) );

		objects[j].bounding_box=boundRect[j];
	}


	/* Save annotations */
	std::cout << "save annotations" << std::endl;
	storeAnnotations(objects, dataset_dir, std::to_string(i)+".xml",std::to_string(i)+".jpg");

	/* Visualize data */
	/*cv::Mat image;
	image = cv::imread("/tmp/camera_utils_output/"+std::to_string(i)+".png", CV_LOAD_IMAGE_COLOR);   // Read the file

	if(! image.data )                              // Check for invalid input
	{
           std::cout <<  "Could not open or find the image" << std::endl ;
           return -1;
	}

	for(int j=0;j<num_objects;++j)
	{
		std::pair <BoundingBox2d::iterator, BoundingBox2d::iterator> ret;
		ret = points_2d.equal_range(objects[j].name);

		for (std::multimap<std::string,ignition::math::Vector2d>::iterator it=ret.first; it!=ret.second; ++it)
		{

			cv::circle(image, cv::Point(it->second.X(),it->second.Y()), 5, cv::Scalar(255,0,1));
		}
       		rectangle( image, boundRect[j].tl(), boundRect[j].br(), cv::Scalar(255,0,1), 2, 8, 0 );
	}
	cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );// Create a window for display.
	cv::imshow( "Display window", image );                   // Show our image inside it.
	cv::waitKey(1000);                                          // Wait for a keystroke in the window*/

	/* Clear the world */
	std::vector<std::string> object_names;
	object_names.push_back("plugin_ground_plane");
	for(int j=0; j < num_objects;++j)
	{
		object_names.push_back(objects[j].name);

	}

        std::cout <<"clean" << std::endl;
	clearWorld(pub_world, object_names);

        while (waitForSpawner(1)){
            usleep(10000);
            queryModelCount(pub_world);
        }//*/


        // TODO - Move camera and light source


	camera_pose=getRandomCameraPose(camera_position);

	world_utils::msgs::WorldUtilsRequest msg_move_basic_objects;
	msg_move_basic_objects.set_type(MOVE);
	//spawnModelFromFile(msg_move_basic_objects, "models/custom_sun.sdf", true, false, false, textures);
	std::string camera_name="custom_camera";
	spawnModelFromFile(msg_move_basic_objects, "models/custom_camera.sdf", false, true, false, textures,  camera_pose.Pos(), camera_pose.Rot(),camera_name);

	pub_world->Publish(msg_move_basic_objects);

    }

    /* Shut down */
    #if GAZEBO_MAJOR_VERSION < 6
    gazebo::shutdown();
    #else
    gazebo::client::shutdown();
    #endif

    return 0;
}


ignition::math::Pose3d getRandomCameraPose(const ignition::math::Vector3d & camera_position) {

        static const ignition::math::Quaternion<double> correct_orientation(ignition::math::Vector3d(0,1,0), -M_PI / 2.0);
	ignition::math::Quaternion<double> camera_orientation(dRand(0,M_PI / 2.0),dRand(0,M_PI / 2.0),dRand(0,M_PI / 2.0)); 

        ignition::math::Pose3d camera_pose;
	camera_pose.Set (camera_position, (correct_orientation*camera_orientation).Inverse());//CoordPositionAdd(camera_position);
	camera_pose=camera_pose.RotatePositionAboutOrigin(camera_orientation);

	return camera_pose;
}

void spawnModelFromFile(
    world_utils::msgs::WorldUtilsRequest & msg,
    const std::string model_path,
    const bool is_light,
    const bool use_custom_pose,
    const bool use_custom_textures,
    std::vector<std::string> textures,
    const ignition::math::Vector3d & position,
    const ignition::math::Quaternion<double> & orientation,
    const std::string &name
    ){

    /* Read model sdf string from file */
    std::ifstream infile {model_path};
    std::string model_sdf { std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>() };
    
    //world_utils::msgs::WorldUtilsRequest msg;

    world_utils::msgs::Object* object = msg.add_object();
    if (is_light){
        object->set_model_type(CUSTOM_LIGHT);
    } else {
        object->set_model_type(CUSTOM);
    }
    object->set_sdf(model_sdf);

    if (use_custom_pose){
        gazebo::msgs::Vector3d *pos = new gazebo::msgs::Vector3d(gazebo::msgs::Convert(position));
        gazebo::msgs::Quaternion *ori = new gazebo::msgs::Quaternion(gazebo::msgs::Convert(orientation));
        gazebo::msgs::Pose *pose = new gazebo::msgs::Pose();
        pose->set_allocated_position(pos);
        pose->set_allocated_orientation(ori);
        object->set_allocated_pose(pose);
    }

    if (use_custom_textures){
        /* Initialize random device */
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_int_distribution<int> dist;
        int j=dist(mt) % textures.size();

        std::string texture = textures.at(j);
        std::stringstream texture_uri;
        std::stringstream texture_name;

        texture_uri << "file://materials/scripts/" << texture << ".material"
        << "</uri><uri>file://materials/textures/";
        texture_name << "Plugin/" << texture;

        object->set_texture_uri(texture_uri.str());
        object->set_texture_name(texture_name.str());
    }

    if(!name.empty())
    {
        object->set_name(name);
    }
}

void spawnRandomObject(
    world_utils::msgs::WorldUtilsRequest & msg,
    std::vector<std::string> textures,
    double & grid_cell_size_x,
    double & grid_cell_size_y,
    int & num_objects,
    std::vector<Object> & objects){

    /* Initialize random device */
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist;

    msg.set_type(SPAWN);

    std::mt19937 g(rd());
    std::shuffle(cells_array.begin(), cells_array.end(), g);
 
	
    for(int i=0; i<num_objects;++i)
    {
            unsigned int x_cell = floor(cells_array[i] / x_cells);
            unsigned int y_cell = floor(cells_array[i] - x_cell * x_cells);
	    std::string object_name;
	    int object_type=dist(mt) % 3;

	    world_utils::msgs::Object* object = msg.add_object();
	    if (object_type == CYLINDER_ID){
		object->set_model_type(CYLINDER);
		object_name="plugin_cylinder_"+std::to_string(cylinder_counter++);
		object->set_name(object_name);
	    }
	    else if(object_type == BOX_ID){
		object->set_model_type(BOX);
		object_name="plugin_box_"+std::to_string(box_counter++);
		object->set_name(object_name);
	    }
	    else if(object_type == SPHERE_ID){
		object->set_model_type(SPHERE);
		object_name="plugin_sphere_"+std::to_string(sphere_counter++);
		object->set_name(object_name);
	    }

	    /* External optional fields have to be allocated */
	    gazebo::msgs::Vector3d *pos = new gazebo::msgs::Vector3d();
	    gazebo::msgs::Quaternion *ori = new gazebo::msgs::Quaternion();
	    gazebo::msgs::Pose *pose = new gazebo::msgs::Pose();
	    gazebo::msgs::Vector3d *size = new gazebo::msgs::Vector3d();
    
	    /* Mass */
	    object->set_mass(dist(mt) % 5 + 1.0);
	    
            /* Sphere/cylinder radius */
	    double radius=dRand(0.1, std::min(grid_cell_size_x,grid_cell_size_y) * 0.5);
	    object->set_radius(radius);


	    
	    /* Cylinder/Sphere length */
	    //object->set_length(z_length);

	    /* Pose */
	    ignition::math::Quaternion<double> object_orientation;

	    if (dRand(0.0, 1.0) < 0.5){
		// Horizontal

	        double x_length(dRand(0.5, 1.0));
	        double y_length(dRand(grid_cell_size_y*0.1, grid_cell_size_y));    
	        double z_length(dRand(grid_cell_size_x*0.1, grid_cell_size_x));

	        size->set_x(x_length);
	        size->set_y(y_length);
	        size->set_z(z_length);

		object_orientation=ignition::math::Quaternion<double> (0.0, M_PI*0.5, dRand(0.0,M_PI));

		if(object_type == CYLINDER_ID || object_type == SPHERE_ID ) {
		   pos->set_z(radius);
	        }
	        else if(object_type == BOX_ID)
		   pos->set_z(x_length*0.5); // height is radius
	    }
            else 
            {
	        // Vertical
	        double x_length(dRand(grid_cell_size_x*0.1, grid_cell_size_x));
	        double y_length(dRand(grid_cell_size_y*0.1, grid_cell_size_y));    
	        double z_length(dRand(0.5, 1.0));

	        size->set_x(x_length);
	        size->set_y(y_length);
	        size->set_z(z_length);

	        double roll = dRand(0.0,M_PI);
	        double pitch = dRand(0.0,M_PI);

	        object_orientation=ignition::math::Quaternion<double> (0.0, 0.0, 0.0);
	        pos->set_z(z_length*0.5);
 	        if(object_type == CYLINDER_ID || object_type == SPHERE_ID) {
		    pos->set_z(radius);
	        }
	    }

	    pos->set_x(x_cell * grid_cell_size_x + 0.5 * grid_cell_size_x - x_cells * 0.5 * grid_cell_size_x);
	    pos->set_y(y_cell * grid_cell_size_y + 0.5 * grid_cell_size_y - y_cells * 0.5 * grid_cell_size_y);

	    ori=new gazebo::msgs::Quaternion(gazebo::msgs::Convert(object_orientation));

	    /* Material script */
	    int j = dist(mt) % textures.size();

	    std::string texture = textures.at(j);
	    std::stringstream texture_uri;
	    std::stringstream texture_name;

	    texture_uri << "file://materials/scripts/" << texture << ".material"
	    << "</uri><uri>file://materials/textures/";
	    texture_name << "Plugin/" << texture;

	    object->set_texture_uri(texture_uri.str());
	    object->set_texture_name(texture_name.str());

	    /* Associate dynamic fields */
	    pose->set_allocated_position(pos);
	    pose->set_allocated_orientation(ori);
	    object->set_allocated_pose(pose);
	    object->set_allocated_box_size(size);

            objects.push_back( Object(object_name,object_type));
    }


}




void clearWorld(gazebo::transport::PublisherPtr pub, std::vector<std::string> object_names){

    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(REMOVE);
    // Only remove models that match the string (exclude custom_camera)

    for(int i(0); i<object_names.size();++i)
    {
        world_utils::msgs::Object* object = msg.add_object();
 	object->set_name(object_names[i]);
    }
    pub->Publish(msg);
}

void changePhysics(gazebo::transport::PublisherPtr pub, bool enable){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(PHYSICS);
    msg.set_state(enable);
    pub->Publish(msg);
}

void pauseWorld(gazebo::transport::PublisherPtr pub, bool enable){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(PAUSE);
    msg.set_state(enable);
    pub->Publish(msg);
}


void captureScene(gazebo::transport::PublisherPtr pub, int j){
    camera_utils::msgs::CameraUtilsRequest msg;
    msg.set_type(CAPTURE_REQUEST);
    msg.set_file_name(std::to_string(j));
    pub->Publish(msg,false);
}

void queryCameraParameters(gazebo::transport::PublisherPtr pub){
    camera_utils::msgs::CameraUtilsRequest msg;
    msg.set_type(CAMERA_INFO_REQUEST);
    pub->Publish(msg,false);
}


/* Handle object count */

bool waitForSpawner(int desired_objects){
    std::lock_guard<std::mutex> lock(object_count_mutex);

    if (desired_objects == object_count)
        return false;
    return true;
}

void queryModelCount(gazebo::transport::PublisherPtr pub){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(STATUS);
    pub->Publish(msg,false);
}

void queryModelBoundingBox(
    gazebo::transport::PublisherPtr pub,
    const std::vector<Object> & objects){
    world_utils::msgs::WorldUtilsRequest msg;
    msg.set_type(STATUS);
    for(int i(0);i<objects.size();++i)
    {
   		world_utils::msgs::BoundingBox* bounding_box = msg.add_bounding_box();
                bounding_box->set_name(objects[i].name);
    }

    pub->Publish(msg,false);
}

void query2DcameraPoint(
    gazebo::transport::PublisherPtr pub,
    const std::vector<Object> & objects
   ){
    	camera_utils::msgs::CameraUtilsRequest msg;
    	msg.set_type(CAMERA_POINT_REQUEST);

	for(int j=0;j<objects.size();++j)
	{
		std::pair <BoundingBox3d::iterator, BoundingBox3d::iterator> ret;
		ret = bbs_3d.equal_range(objects[j].name);

		for (BoundingBox3d::iterator it=ret.first; it!=ret.second; ++it)
		{

			ignition::math::Vector3d point_1=it->second.center; point_1.X()+=it->second.size.X()*0.5; point_1.Y()+=it->second.size.Y()*0.5; point_1.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_2=it->second.center; point_2.X()+=it->second.size.X()*0.5; point_2.Y()+=it->second.size.Y()*0.5; point_2.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_3=it->second.center; point_3.X()+=it->second.size.X()*0.5; point_3.Y()-=it->second.size.Y()*0.5; point_3.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_4=it->second.center; point_4.X()+=it->second.size.X()*0.5; point_4.Y()-=it->second.size.Y()*0.5; point_4.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_5=it->second.center; point_5.X()-=it->second.size.X()*0.5; point_5.Y()+=it->second.size.Y()*0.5; point_5.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_6=it->second.center; point_6.X()-=it->second.size.X()*0.5; point_6.Y()+=it->second.size.Y()*0.5; point_6.Z()-=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_7=it->second.center; point_7.X()-=it->second.size.X()*0.5; point_7.Y()-=it->second.size.Y()*0.5; point_7.Z()+=it->second.size.Z()*0.5;
			ignition::math::Vector3d point_8=it->second.center; point_8.X()-=it->second.size.X()*0.5; point_8.Y()-=it->second.size.Y()*0.5; point_8.Z()-=it->second.size.Z()*0.5;

			/* point 1 */
			gazebo::msgs::Vector3d *point_msg_1 = new gazebo::msgs::Vector3d();
			point_msg_1->set_x(point_1.X());
			point_msg_1->set_y(point_1.Y());
			point_msg_1->set_z(point_1.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_1 = msg.add_bounding_box();
			bounding_box_1->set_name(objects[j].name);
			bounding_box_1->set_allocated_point3d(point_msg_1);

			/* point 2 */
			gazebo::msgs::Vector3d *point_msg_2 = new gazebo::msgs::Vector3d();
			point_msg_2->set_x(point_2.X());
			point_msg_2->set_y(point_2.Y());
			point_msg_2->set_z(point_2.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_2 = msg.add_bounding_box();
			bounding_box_2->set_name(objects[j].name);
			bounding_box_2->set_allocated_point3d(point_msg_2);

			/* point 3 */
			gazebo::msgs::Vector3d *point_msg_3 = new gazebo::msgs::Vector3d();
			point_msg_3->set_x(point_3.X());
			point_msg_3->set_y(point_3.Y());
			point_msg_3->set_z(point_3.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_3 = msg.add_bounding_box();
			bounding_box_3->set_name(objects[j].name);
			bounding_box_3->set_allocated_point3d(point_msg_3);

			/* point 4 */
			gazebo::msgs::Vector3d *point_msg_4 = new gazebo::msgs::Vector3d();
			point_msg_4->set_x(point_4.X());
			point_msg_4->set_y(point_4.Y());
			point_msg_4->set_z(point_4.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_4 = msg.add_bounding_box();
			bounding_box_4->set_name(objects[j].name);
			bounding_box_4->set_allocated_point3d(point_msg_4);

			/* point 5 */
			gazebo::msgs::Vector3d *point_msg_5 = new gazebo::msgs::Vector3d();
			point_msg_5->set_x(point_5.X());
			point_msg_5->set_y(point_5.Y());
			point_msg_5->set_z(point_5.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_5 = msg.add_bounding_box();
			bounding_box_5->set_name(objects[j].name);
			bounding_box_5->set_allocated_point3d(point_msg_5);

			/* point 6 */
			gazebo::msgs::Vector3d *point_msg_6 = new gazebo::msgs::Vector3d();
			point_msg_6->set_x(point_6.X());
			point_msg_6->set_y(point_6.Y());
			point_msg_6->set_z(point_6.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_6 = msg.add_bounding_box();
			bounding_box_6->set_name(objects[j].name);
			bounding_box_6->set_allocated_point3d(point_msg_6);

			/* point 7 */
			gazebo::msgs::Vector3d *point_msg_7 = new gazebo::msgs::Vector3d();
			point_msg_7->set_x(point_7.X());
			point_msg_7->set_y(point_7.Y());
			point_msg_7->set_z(point_7.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_7 = msg.add_bounding_box();
			bounding_box_7->set_name(objects[j].name);
			bounding_box_7->set_allocated_point3d(point_msg_7);

			/* point 8 */
			gazebo::msgs::Vector3d *point_msg_8 = new gazebo::msgs::Vector3d();
			point_msg_8->set_x(point_8.X());
			point_msg_8->set_y(point_8.Y());
			point_msg_8->set_z(point_8.Z());

	    		camera_utils::msgs::BoundingBoxCamera* bounding_box_8 = msg.add_bounding_box();
			bounding_box_8->set_name(objects[j].name);
			bounding_box_8->set_allocated_point3d(point_msg_8);
		}
	}

	pub->Publish(msg,false);
}

void onWorldUtilsResponse(WorldUtilsResponsePtr &_msg){
    if (_msg->type() == INFO){
        if (_msg->has_object_count()){
            std::lock_guard<std::mutex> lock(object_count_mutex);
            object_count = _msg->object_count();
        }
    } else if (_msg->type() == PROPERTIES){

        for(int i(0); i<_msg->bounding_box_size();++i){
            ignition::math::Vector3d bb_center = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).bb_center());
            ignition::math::Vector3d bb_size = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).bb_size());

            bounding_box_3d bb(bb_center, bb_size);
	    bbs_3d.insert( std::pair<std::string,bounding_box_3d>(_msg->bounding_box(i).name(), bb) );
        }
    }
}

/* Handle camera success */
bool waitForCamera(){

    std::lock_guard<std::mutex> lock(camera_success_mutex);

    if (camera_success){
        camera_success = false;
        return false;
    }
    return true;
}


void onCameraUtilsResponse(CameraUtilsResponsePtr &_msg){

    if (_msg->type()==CAMERA_POINT_RESPONSE){

        for(int i(0); i<_msg->bounding_box_size();++i){
        	ignition::math::Vector2d point_2d = gazebo::msgs::ConvertIgn(_msg->bounding_box(i).point());

    		points_2d.insert( std::pair<std::string,ignition::math::Vector2d>(_msg->bounding_box(i).name(), point_2d) );
	}
    }
    else if (_msg->type()==CAPTURE_RESPONSE){
        std::lock_guard<std::mutex> lock(camera_success_mutex);

	if(_msg->success())
	{
    		std::cout << "capture response" << std::endl;
       		camera_success = true;    
	}
    }
    else if(_msg->type()==CAMERA_INFO_RESPONSE)
    {
        std::lock_guard<std::mutex> lock(camera_success_mutex);

	if(_msg->success())
	{
		std::cout << "camera info response" << std::endl;
       		camera_success = true;   
		camera_info=std::shared_ptr<CameraInfo> (new CameraInfo(_msg->camera_info().width(), _msg->camera_info().height(), _msg->camera_info().depth()) );
	}
    }
    else {
        std::cout << "Camera could not save to file! Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
}


void storeAnnotations(const std::vector<Object> & objects, const std::string & path, const std::string & file_name, const std::string & image_name)
{
	std::ofstream out(path+"Annotations"+file_name);
        out << "<annotation>" << std::endl 
	    << "  <folder>images</folder>" << std::endl
	    << "  <filename>"+image_name+"</filename>" << std::endl
            << "  <source>"<<std::endl
	    << "    <database>The SHAPE2017 Database</database>"<< std::endl
	    << "    <annotation>SHAPE SHAPE2017</annotation>" << std::endl 
            << "    <image>"+ image_name +"</image>" << std::endl
            << "  </source>" << std::endl
            << "  <size>" << std::endl
            << "    <width>"  << camera_info->width  << "</width>"  << std::endl
            << "    <height>" << camera_info->height << "</height>" << std::endl
            << "    <depth>"  << camera_info->depth  << "</depth>"  << std::endl
            << "  </size>" << std::endl
            << "  <segmented>1</segmented>" << std::endl;


	for(unsigned int i=0; i<objects.size(); ++i) 
	{
		out << "  <object>" << std::endl
		    << "    <name>" << classes_map.find(objects[i].type)->second << "</name>" << std::endl
		    << "    <pose>top</pose>" << std::endl
		    << "    <truncated>0</truncated>" << std::endl
		    << "    <difficult>1</difficult>" << std::endl
		    << "    <bndbox>" << std::endl
		    << "      <xmin>"<< objects[i].bounding_box.x <<"</xmin>" << std::endl
		    << "      <ymin>"<< objects[i].bounding_box.y <<"</ymin>" << std::endl
		    << "      <xmax>"<< objects[i].bounding_box.x + objects[i].bounding_box.width <<"</xmax>" << std::endl
		    << "      <ymax>"<< objects[i].bounding_box.y + objects[i].bounding_box.height <<"</ymax>" << std::endl
		    << "    </bndbox>" << std::endl
		    << "  </object>" << std::endl;
	}

	out << "</annotation>";
        out.close();
}
