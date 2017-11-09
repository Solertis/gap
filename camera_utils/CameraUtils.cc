/**
 * @file CameraUtils.hh
 * @brief Camera utils plugin implementation
 *
 * A custom gazebo plugin that provides an interface to programatically collect data from cameras
 * at specific times.
 *
 * @author João Borrego
 */

#include "CameraUtils.hh"

namespace gazebo {

    /* Register this plugin with the simulator */
    GZ_REGISTER_SENSOR_PLUGIN(CameraUtils)

    /**
     * @brief      Class for private camera utils plugin data.
     */
    class CameraUtilsPrivate
    {
        public:

            /** Gazebo transport node */
            public: transport::NodePtr node;
            /** Camera utils topic subscriber */
            public: transport::SubscriberPtr sub;
            /** Camera utils topic publisher */
            public: transport::PublisherPtr pub;
    };

    CameraUtils::CameraUtils()
        : SensorPlugin(), dataPtr(new CameraUtilsPrivate){

        std::cout << "[CameraUtils] Loaded camera tools." << std::endl;
    }

    CameraUtils::~CameraUtils(){
        this->newFrameConnection.reset();
        this->parentSensor.reset();
        this->camera.reset();
        this->dataPtr->sub.reset();
        this->dataPtr->node->Fini();
        std::cout << "[CameraUtils] Unloaded camera tools." << std::endl;
    }

    void CameraUtils::Load(sensors::SensorPtr _sensor, sdf::ElementPtr _sdf){

        /* Check if a parent sensor is provided */
        if (!_sensor)
            gzerr << "[CameraUtils] Invalid sensor pointer." << std::endl;

        /* Camera sensor */
        this->parentSensor =
            std::dynamic_pointer_cast<sensors::CameraSensor>(_sensor);
        this->camera = this->parentSensor->Camera();
        this->width = this->camera->ImageWidth();
        this->height = this->camera->ImageHeight();
        this->depth = this->camera->ImageDepth();
        this->format = this->camera->ImageFormat();

        /* Plugin parameters */

        if (_sdf->HasElement("output_dir")){
            this->output_dir = _sdf->Get<std::string>("output_dir");
        } else {
            this->output_dir = DEFAULT_OUTPUT_DIR;
        }
        if (_sdf->HasElement("extension")){
            this->extension = _sdf->Get<std::string>("extension");
        } else {
            this->extension = DEFAULT_EXTENSION;
        }

        /* Subscriber setup */
        this->dataPtr->node = transport::NodePtr(new transport::Node());
        this->dataPtr->node->Init();

        /* Create a topic for listening to requests */
        std::string topic_name = REQUEST_TOPIC;
        /* Subcribe to the topic */
        this->dataPtr->sub = this->dataPtr->node->Subscribe(topic_name,
            &CameraUtils::onRequest, this);
        /* Setup publisher for the reply topic */
        this->dataPtr->pub = this->dataPtr->node->
            Advertise<camera_utils::msgs::CameraUtilsResponse>(RESPONSE_TOPIC);

        /* Create output directory */
        boost::filesystem::path dir(output_dir);
        boost::filesystem::create_directories(dir);

        this->newFrameConnection = this->camera->ConnectNewImageFrame(
            std::bind(&CameraUtils::OnNewFrame, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
            std::placeholders::_4, std::placeholders::_5));

        this->parentSensor->SetActive(true);
    }

    void CameraUtils::onRequest(CameraUtilsRequestPtr &_msg){

        std::string file_name;

        if (_msg->type() == CAPTURE){

            if (_msg->has_file_name()){
                file_name = _msg->file_name() + extension;
            } else {
                file_name = std::to_string(saved_counter++) + extension;
            }

            this->next_file_name = output_dir + file_name;
            this->save_on_update = true;
        }
	else if(_msg->type() == CAMERA_POINT)
	{
        	if (_msg->has_point()){
            	    camera_utils::msgs::CameraUtilsResponse msg;
		    ignition::math::Vector3d point_3d = gazebo::msgs::ConvertIgn(_msg->point());

		    ignition::math::Vector2i point_2d = this->camera->Project (point_3d);

                    gazebo::msgs::Vector2d *point_2d_msg = new gazebo::msgs::Vector2d();

                    point_2d_msg->set_x(point_2d.X());
                    point_2d_msg->set_y(point_2d.Y());

		    msg.set_success(true);
		    msg.set_name(_msg->name());
		    msg.set_allocated_point(point_2d_msg);

		    this->dataPtr->pub->Publish(msg);
        	}
	}
    }

    void CameraUtils::OnNewFrame(
        const unsigned char *   /*_image*/,
        unsigned int            /*_width*/,
        unsigned int            /*_height*/,
        unsigned int            /*_depth*/,
        const std::string &     /*_format*/){

        if (save_on_update){

            save_on_update = false;

            bool success = this->camera->SaveFrame(next_file_name);
            std::cout << "[CameraUtils] Saving frame as [" <<next_file_name << "]" << std::endl;

            camera_utils::msgs::CameraUtilsResponse msg;
            msg.set_success(success);
            this->dataPtr->pub->Publish(msg);
        }
    }
}
