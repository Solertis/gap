/// \file world_utils/WorldUtils.cc
/// \brief TODO
///
/// TODO
///
/// \author João Borrego

#include "WorldUtils.hh"

namespace gazebo {

// Register this plugin with the simulator
GZ_REGISTER_WORLD_PLUGIN(WorldUtils)

WorldUtils::WorldUtils() : WorldPlugin(){
    gzmsg << "[WorldUtils] Loaded world tools." << std::endl;
}

void WorldUtils::Load(physics::WorldPtr _world, sdf::ElementPtr _sdf){

    // Plugin parameters
    this->world = _world;

    // Subscriber setup
    this->node = transport::NodePtr(new transport::Node());
    this->node->Init(this->world->Name());

    // Setup publisher for the gazebo request topic
    this->request_pub = this->node->Advertise<msgs::Request>("~/request");

    // Subcribe to the request topic
    this->sub = this->node->Subscribe(REQUEST_TOPIC, &WorldUtils::onRequest, this);
    // Setup publisher for the response topic
    this->pub = this->node->Advertise<world_utils::msgs::WorldUtilsResponse>(RESPONSE_TOPIC);

    // Setup regular expression used for texture replacement
    this->script_reg = std::regex(REGEX_XML_SCRIPT);
    // Setup regular expression used for pose replacement
    this->pose_reg = std::regex(REGEX_XML_POSE);

    // Connect to the world update signal
    this->updateConnection = event::Events::ConnectPreRender(
        std::bind(&WorldUtils::onUpdate, this));
}

/////////////////////////////////////////////////
void WorldUtils::onUpdate()
{
    bool moved = false;    
    world_utils::msgs::WorldUtilsResponse msg;

    std::lock_guard<std::mutex> lock(this->mutex);

    // Move objects
    while (!this->type.empty())
    {
        int model_type = this->type.front();
        std::string name = this->move.front();
        ignition::math::Pose3d pose = this->poses.front();

        if (model_type == CUSTOM_LIGHT) {
            physics::LightPtr light = this->world->LightByName(name);
            if (light) light->SetWorldPose(pose);
        } else {
            physics::ModelPtr model = this->world->ModelByName(name);
            if (model) model->SetWorldPose(pose);
        }
        type.pop(); move.pop(); poses.pop();

        moved = true;
    }

    // Report sucess in move
    if (moved) {
        msg.set_type(SUCCESS);
        this->pub->Publish(msg);
    }
}

/////////////////////////////////////////////////
void WorldUtils::onRequest(WorldUtilsRequestPtr &_msg){

    /// TODO - better structure

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

    if (type == SPAWN){

        for (int i = 0; i < _msg->object_size(); i++){
            model_type = (_msg->object(i).has_model_type())?
                (_msg->object(i).model_type()) : -1;

            /// Extract parameters from message
            if (_msg->object(i).has_pose()){
                pos = msgs::ConvertIgn(_msg->object(i).pose().position());
                ori = msgs::ConvertIgn(_msg->object(i).pose().orientation());
            }
            if (_msg->object(i).has_mass()){
                mass = _msg->object(i).mass();
            }

            if (model_type == SPHERE){

                name = _msg->object(i).has_name()?
                    _msg->object(i).name() : "plugin_sphere_" + std::to_string(this->sphere_counter++);
                radius = _msg->object(i).has_radius()?
                    _msg->object(i).radius() : 1.0;

                sdf_string = genSphere(name, mass, radius, pos, ori);

            } else if (model_type == CYLINDER){

                name = _msg->object(i).has_name()?
                    _msg->object(i).name() : "plugin_cylinder_" + std::to_string(this->cylinder_counter++);
                radius = _msg->object(i).has_radius()?
                    _msg->object(i).radius() : 1.0;
                length = _msg->object(i).has_length()?
                    _msg->object(i).length() : 1.0;

                sdf_string = genCylinder(name, mass, radius, length, pos, ori);

            } else if (model_type == BOX){

                name = _msg->object(i).has_name()?
                    _msg->object(i).name() : "plugin_box_" + std::to_string(this->box_counter++);
                if (_msg->object(i).has_box_size())
                    box_size = msgs::ConvertIgn(_msg->object(i).box_size());

                sdf_string = genBox(name, mass, box_size, pos, ori);

            } else if (model_type == CUSTOM || model_type == CUSTOM_LIGHT){

                sdf_string = _msg->object(i).has_sdf()?
                    _msg->object(i).sdf() : "";

            } else if (model_type == MODEL){

                if (_msg->object(i).has_name()){
                    name = "model://" +_msg->object(i).name();
                    this->world->InsertModelFile(name);
                }
            }

            /// If a spawn message was requested
            if (!sdf_string.empty()){

                std::ostringstream model_str;

                if (model_type != CUSTOM && model_type != CUSTOM_LIGHT) {
                    /// Enclose in sdf xml tags
                    model_str << "<sdf version='" << SDF_VERSION << "'>"
                    << sdf_string << "</sdf>";

                } else {

                    /// Regex to modify pose string in custom model
                    if (_msg->object(i).has_pose()){

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

                if (_msg->object(i).has_texture_uri() && _msg->object(i).has_texture_name()){

                    /// Change material script in string
                    texture_uri = _msg->object(i).texture_uri();
                    texture_name = _msg->object(i).texture_name();

                    std::string texture_str =
                        "<script><uri>" + texture_uri + "</uri>" +
                        "<name>" + texture_name + "</name></script>";

                    new_model_str = std::regex_replace(
                        model_str.str(), this->script_reg, texture_str);

                } else {
                    new_model_str = model_str.str();
                }

                // Insert model in World
                sdf::SDF objectSDF;
                objectSDF.SetFromString(new_model_str);
                this->world->InsertModelSDF(objectSDF);
            }
        }

    } else if (type == MOVE) {

        for (int i = 0; i < _msg->object_size(); i++)
        {
            model_type = (_msg->object(i).has_model_type())? (_msg->object(i).model_type()) : -1;

            if (_msg->object(i).has_name() && _msg->object(i).has_pose()) {
                std::string name(_msg->object(i).name());
                msgs::Pose m_pose = _msg->object(i).pose();
                ignition::math::Pose3d pose(msgs::ConvertIgn(m_pose));

                std::lock_guard<std::mutex> lock(this->mutex);

                this->type.push(model_type);
                this->move.push(name);
                this->poses.push(pose);
                
            }
        }

    } else if (type == REMOVE) {

        if(_msg->object_size() > 0) {

            for (int i = 0; i < _msg->object_size(); i++) {
                model_type = (_msg->object(i).has_model_type())? (_msg->object(i).model_type()) : -1;
                if (_msg->object(i).has_name()){
                    /// Clear specific object(s)
                    clearMatching(_msg->object(i).name() , (model_type == CUSTOM_LIGHT));
                } else {
                    /// Clear everything
                    clearWorld();
                }
            }
        } else {
            clearWorld();
        }
    }

    else if (type == PHYSICS) {

            bool state = (_msg->has_state())?
                _msg->state() : !this->world->PhysicsEnabled();
            this->world->SetPhysicsEnabled(state);

    } else if (type == PAUSE){

        bool state = (_msg->has_state())?
            _msg->state() : !this->world->IsPaused();;
        this->world->SetPaused(state);

    } else if (type == STATUS){

        // Return total count of models and lights in the world
        world_utils::msgs::WorldUtilsResponse msg;
        int model_count = this->world->ModelCount();
        int light_count = this->world->LightCount();
        msg.set_type(INFO);
        msg.set_object_count(model_count + light_count);
        pub->Publish(msg,true);
    }
}

/////////////////////////////////////////////////
void WorldUtils::clearWorld(){

    this->world->Clear();
}

/////////////////////////////////////////////////
void WorldUtils::clearMatching(const std::string &match, const bool is_light){

    std::string entity_name;
    std::string match_str = match;
    gazebo::msgs::Request *msg;

    if (is_light){

        physics::Light_V lights = this->world->Lights();
        for (auto &l : lights){
            entity_name = l->GetName();
            if (entity_name.find(match_str) != std::string::npos){
                msg = gazebo::msgs::CreateRequest("entity_delete", entity_name);
                request_pub->Publish(*msg, true);
            }
        }

    } else {

        physics::Model_V models = this->world->Models();
        for (auto &m : models){
            entity_name = m->GetName();
            if (entity_name.find(match_str) != std::string::npos){
                msg = gazebo::msgs::CreateRequest("entity_delete", entity_name);
                request_pub->Publish(*msg, true);
            }
        }
    }

    delete msg;
}

/////////////////////////////////////////////////
const std::string WorldUtils::genSphere(
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

/////////////////////////////////////////////////
const std::string WorldUtils::genCylinder(
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

/////////////////////////////////////////////////
const std::string WorldUtils::genBox(
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

}
