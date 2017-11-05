/**
 * @file object_spawner.cc 
 * @brief Custom factory plugin for gazebo with own interface
 *
 * @author João Borrego
 */

#include "object_spawner.hh"

namespace gazebo
{
    ObjectSpawnerPlugin::ObjectSpawnerPlugin() : WorldPlugin(){
        std::cout << "[PLUGIN] Loaded object spawner." << std::endl;
    }

    void ObjectSpawnerPlugin::Load(physics::WorldPtr _world, sdf::ElementPtr _sdf){

        /* Plugin parameters */
        this->world = _world;

        /* Subscriber setup */
        this->node = transport::NodePtr(new transport::Node());

        #if GAZEBO_MAJOR_VERSION < 8
        this->node->Init(this->world->GetName());
        #else
        this->node->Init(this->world->Name());
        #endif

        /* Setup publisher for the factory topic */
        this->factory_pub = this->node->Advertise<msgs::Factory>("~/factory");
        /* Setup publisher for the light factory topic */
        this->factory_light_pub = this->node->Advertise<msgs::Light>("~/factory/light");
        /* Setup publisher for the gazebo request topic */
        this->request_pub = this->node->Advertise<msgs::Request>("~/request");
        
        /* Subcribe to the request topic */
        this->sub = this->node->Subscribe(REQUEST_TOPIC, &ObjectSpawnerPlugin::onMsg, this);
        /* Setup publisher for the response topic */
        this->pub = this->node->Advertise<object_spawner_msgs::msgs::Reply>(RESPONSE_TOPIC);
    
        /* Setup regular expression used for texture replacement */
        this->script_reg = std::regex(REGEX_XML_SCRIPT);
        /* Setup regular expression used for pose replacement */
        this->pose_reg = std::regex(REGEX_XML_POSE);
    }

    /* Private methods */

    void ObjectSpawnerPlugin::onMsg(SpawnRequestPtr &_msg){

        int type;
        int model_type;
        std::string name;
        ignition::math::Vector3d pos(0,0,0);
        ignition::math::Quaterniond ori(0,0,0,0);
        double mass;
        std::string texture_uri;
        std::string texture_name;
        double radius;
        double length;
        ignition::math::Vector3d box_size(0,0,0);
        
        std::string sdf_string;

        type = (_msg->has_type())? (_msg->type()) : -1;
        model_type = (_msg->has_model_type())? (_msg->model_type()) : -1;

        if (type == SPAWN){
            
            /* Extract parameters from message */
            if (_msg->has_pose()){
                pos = msgs::ConvertIgn(_msg->pose().position());
                ori = msgs::ConvertIgn(_msg->pose().orientation());
            }
            if (_msg->has_mass()){
                mass = _msg->mass();
            }

            if (model_type == SPHERE){
            
                name = _msg->has_name()?
                    _msg->name() : "plugin_sphere_" + std::to_string(this->sphere_counter++);
                radius = _msg->has_radius()?
                    _msg->radius() : 1.0;

                sdf_string = genSphere(name, mass, radius, pos, ori);
            
            } else if (model_type == CYLINDER){
                
                name = _msg->has_name()?
                    _msg->name() : "plugin_cylinder_" + std::to_string(this->cylinder_counter++);
                radius = _msg->has_radius()?
                    _msg->radius() : 1.0;
                length = _msg->has_length()?
                    _msg->length() : 1.0;

                sdf_string = genCylinder(name, mass, radius, length, pos, ori);
            
            } else if (model_type == BOX){
                
                name = _msg->has_name()?
                    _msg->name() : "plugin_box_" + std::to_string(this->box_counter++);
                if (_msg->has_box_size())
                    box_size = msgs::ConvertIgn(_msg->box_size());

                sdf_string = genBox(name, mass, box_size, pos, ori);
            
            } else if (model_type == CUSTOM || model_type == CUSTOM_LIGHT){

                sdf_string = _msg->has_sdf()?
                    _msg->sdf() : "";
            
            } else if (model_type == MODEL){

                if (_msg->has_name()){
                    name = "model://" + _msg->name();
                    this->world->InsertModelFile(name);                
                }

            } else if (model_type == GROUND){

                this->world->InsertModelFile("model://ground_plane");
            }

            /* If a spawn message was requested */
            if (!sdf_string.empty()){

                std::ostringstream model_str;
                    
                if (model_type != CUSTOM && model_type != CUSTOM_LIGHT) {
                    /* Enclose in sdf xml tags */
                    model_str << "<sdf version='" << SDF_VERSION << "'>"
                    << sdf_string << "</sdf>";
                
                } else {
                    
                    /* Regex to modify pose string in custom model */
                    if (_msg->has_pose()){

                        ignition::math::Vector3d rpy = ori.Euler();

                        std::ostringstream pose_xml;
                        pose_xml << 
                            "<pose>" << 
                            pos.X() << " " << pos.Y() << " " << pos.Z() << " " <<
                            rpy.X() << " " << rpy.Y() << " " << rpy.Z() <<
                            "</pose>";

                        std::string new_model_str = std::regex_replace(
                            sdf_string, this->pose_reg, pose_xml.str());
                        
                        model_str << new_model_str;
                    
                    } else {
                        model_str << sdf_string;
                    }
                }

                std::string new_model_str;

                if (_msg->has_texture_uri() && _msg->has_texture_name()){

                    /* Change material script in string */
                    texture_uri = _msg->texture_uri();
                    texture_name = _msg->texture_name();

                    std::string texture_str =
                    "<script><uri>" + texture_uri + "</uri>" +
                    "<name>" + texture_name + "</name></script>";

                    new_model_str = std::regex_replace(
                        model_str.str(), this->script_reg, texture_str);
                
                } else {
                    new_model_str = model_str.str();
                } 

                /* Send the model to the gazebo factory */
                if (model_type == CUSTOM_LIGHT) {
                    sdf::SDF sdf_light;
                    sdf_light.SetFromString(new_model_str);
                    msgs::Light msg = msgs::LightFromSDF(sdf_light.Root()->GetElement("light"));
                    msg.set_name("plugin_light");
                    this->factory_light_pub->Publish(msg);
                } else {
                    msgs::Factory msg;
                    msg.set_sdf(new_model_str);
                    this->factory_pub->Publish(msg);
                }
                
            }

        } else if (type == MOVE) {

            if (_msg->has_name() && _msg->has_pose()){
                
                msgs::Pose m_pose = _msg->pose();
                ignition::math::Pose3d pose = msgs::ConvertIgn(m_pose);
                physics::ModelPtr model = this->world->GetModel(_msg->name());
                model-> SetWorldPose(pose);
            }

        } else if (type == CLEAR){

            if (_msg->has_name()){
                /* Clear specific object(s) */
                clearMatching(_msg->name());
            } else {
                /* Clear everything */
                clearWorld();
            }
        
        } else if (type == TOGGLE){
            
            bool state = (_msg->has_state())?
                _msg->state() : !this->world->GetEnablePhysicsEngine();
            this->world->EnablePhysicsEngine(state);

        } else if (type == PAUSE){
            
            bool state = (_msg->has_state())?
                _msg->state() : !this->world->IsPaused();;
            this->world->SetPaused(state);

        } else if (type == STATUS){

            int model_count = this->world->GetModelCount();
            
            object_spawner_msgs::msgs::Reply msg;
            msg.set_type(INFO);
            msg.set_object_count(model_count);
            pub->Publish(msg);
        }
    }

    void ObjectSpawnerPlugin::clearWorld(){

        this->world->Clear();
    }

    void ObjectSpawnerPlugin::clearMatching(const std::string &match){

        int model_count = this->world->GetModelCount();
        std::string model_name;
        std::string match_str = match;
        gazebo::msgs::Request *msg;

        for (int idx = 0; idx < model_count; idx++){
            physics::ModelPtr model = this->world->GetModel(idx);
            model_name = model->GetName();
            if (model_name.find(match_str) != std::string::npos){
                msg = gazebo::msgs::CreateRequest("entity_delete", model_name);
                request_pub->Publish(*msg, true);
            }
        }

        delete msg;
    }

    const std::string ObjectSpawnerPlugin::genSphere(
        const std::string &model_name,
        const double mass,
        const double radius,
        const ignition::math::Vector3d position,
        const ignition::math::Quaterniond orientation){

        msgs::Model model;
        model.set_name(model_name);
        msgs::Set(model.mutable_pose(),
            ignition::math::Pose3d(position, orientation));
        msgs::AddSphereLink(model, mass, radius);

        return msgs::ModelToSDF(model)->ToString("");
    }

    const std::string ObjectSpawnerPlugin::genCylinder(
        const std::string &model_name,
        const double mass,
        const double radius,
        const double length,
        const ignition::math::Vector3d position,
        const ignition::math::Quaterniond orientation){
        
        msgs::Model model;
        model.set_name(model_name);
        msgs::Set(model.mutable_pose(),
            ignition::math::Pose3d(position, orientation));
        msgs::AddCylinderLink(model, mass, radius, length);

        return msgs::ModelToSDF(model)->ToString("");
    };

    const std::string ObjectSpawnerPlugin::genBox(
        const std::string &model_name,
        const double mass,
        const ignition::math::Vector3d size,
        const ignition::math::Vector3d position,
        const ignition::math::Quaterniond orientation){
        
        msgs::Model model;
        model.set_name(model_name);
        msgs::Set(model.mutable_pose(),
            ignition::math::Pose3d(position, orientation));
        msgs::AddBoxLink(model, mass, size);

        return msgs::ModelToSDF(model)->ToString("");
    };

    GZ_REGISTER_WORLD_PLUGIN(ObjectSpawnerPlugin)
}