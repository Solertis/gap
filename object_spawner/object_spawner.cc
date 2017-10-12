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

        this->world = _world;

        /* Subscriber setup */
        this->node = transport::NodePtr(new transport::Node());

        #if GAZEBO_MAJOR_VERSION < 8
        this->node->Init(this->world->GetName());
        #else
        this->node->Init(this->world->Name());
        #endif

        /* Create a topic for listening to requests */
        std::string topic_name = OBJECT_SPAWNER_TOPIC;
        /* Subcribe to the topic */
        this->sub = this->node->Subscribe(topic_name,
            &ObjectSpawnerPlugin::onMsg, this);

        /* Setup publisher for the factory topic */
        this->factory_pub = this->node->Advertise<msgs::Factory>("~/factory");
    
        /* Setup regular expression used for texture replacement */
        this->script_reg = std::regex("<script>[\\s\\S]*?<\\/script>");
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

        type = (_msg->type())? (_msg->type()) : -1;
        model_type = (_msg->model_type())? (_msg->model_type()) : -1;

        if(type == SPAWN){
            
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
            }

            /* If a spawn message was requested */
            if (!sdf_string.empty()){

                /* Enclose in sdf xml tags */
                std::ostringstream model_str;
                model_str << "<sdf version='" << SDF_VERSION << "'>"
                << sdf_string << "</sdf>";
                
                /* Change material script in string */
                texture_uri = _msg->has_texture_uri()?
                    _msg->texture_uri() : "file://media/materials/scripts/gazebo.material";
                texture_name = _msg->has_texture_name()?
                    _msg->texture_name() : "Gazebo/White";

                std::string texture_str =
                "<script><uri>" + texture_uri + "</uri>" +
                "<name>" + texture_name + "</name></script>";

                const std::string new_model_str = std::regex_replace(
                    model_str.str(), this->script_reg, texture_str);
                
                /* Send the model to the gazebo server */
                msgs::Factory msg;
                msg.set_sdf(new_model_str);
                this->factory_pub->Publish(msg);
            }

        } else if (type == REMOVE){

            // TODO
        }
    }

    void ObjectSpawnerPlugin::printLiveObjs(){

        std::cout << "[PLUGIN] Printing live object list" << std::endl;

        int model_count = this->world->GetModelCount();
        std::cout << model_count << std::endl;

        for (int idx = 0; idx < model_count; idx++){
            physics::ModelPtr model = this->world->GetModel(idx);
            std::cout << model->GetName() << std::endl;
        }
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