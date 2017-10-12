/**
 * @file object_spawner.hh 
 * @brief headers for custom factory plugin for gazebo with own interface
 *
 * @author João Borrego
 */

#include <iostream>         // io
#include <iomanip>          // setprecision
#include <sstream>          // stringstream
#include <list>             // list of live objects 
#include <string>           // strings
#include <regex>            // regular expressions
#include <gazebo/gazebo.hh> // gazebo

#include "gazebo/physics/physics.hh"
#include "gazebo/common/common.hh"
#include "gazebo/gazebo.hh"

#include <gazebo/transport/transport.hh>
#include <gazebo/msgs/msgs.hh>

/* Custom messages */
#include "object_spawner_request.pb.h"

/** @brief Topic monitored for incoming commands */
#define OBJECT_SPAWNER_TOPIC "~/gazebo-utils/object_spawner"

/* Ease of use macros */

/** @brief Spawn object request */
#define SPAWN       object_spawner_msgs::msgs::SpawnRequest::SPAWN
/** @brief Remove object request */
#define REMOVE      object_spawner_msgs::msgs::SpawnRequest::REMOVE
/** @brief Spawn sphere object */
#define SPHERE      object_spawner_msgs::msgs::SpawnRequest::SPHERE
/** @brief Spawn cylinder object */
#define CYLINDER    object_spawner_msgs::msgs::SpawnRequest::CYLINDER
/** @brief Spawn box object */
#define BOX         object_spawner_msgs::msgs::SpawnRequest::BOX

namespace gazebo {

    typedef const boost::shared_ptr<const object_spawner_msgs::msgs::SpawnRequest>
        SpawnRequestPtr;

    class ObjectSpawnerPlugin : public WorldPlugin {

        /* Private attributes */
        private:

            /* @brief A pointer to the world */
            physics::WorldPtr world;
            /* @brief Keep track of live objects */
            std::list<std::string> live_objs;
            /* @brief A node used for transport */
            transport::NodePtr node;
            /* @brief A subscriber to a named topic */
            transport::SubscriberPtr sub;
            /* @brief A publisher to the factory topic */
            transport::PublisherPtr factory_pub;

            std::regex script_reg;

            /* Counters for automatic naming */
            int sphere_counter = 0;
            int cylinder_counter = 0;
            int box_counter = 0;
        
        /* Public methods */
        public:
            
            /**
             * @brief      Constructor
             */
            ObjectSpawnerPlugin();

            /**
             * @brief      Plugin setup executed on gazebo server launch
             *
             * @param[in]  _world  The world pointer
             * @param[in]  _sdf    The sdf parameters
             */
            void Load(physics::WorldPtr _world, sdf::ElementPtr _sdf);

        /* Private methods */
        private:

            /**
             * @brief      Callback function for receiving a message
             *
             * @param      _msg  The message
             */
            void onMsg(SpawnRequestPtr &_msg);

            /**
             * @brief      Prints live objects in the world
             */
            void printLiveObjs();

            /**
             * @brief      Generates SDF string for sphere object
             *
             * @param[in]  model_name   The model name
             * @param[in]  mass         The mass
             * @param[in]  radius       The radius
             * @param[in]  position     The position
             * @param[in]  orientation  The orientation
             *
             * @return     The sphere SDF string
             */
            const std::string genSphere(
                const std::string &model_name,
                const double mass,
                const double radius,
                const ignition::math::Vector3d position,
                const ignition::math::Quaterniond orientation);

            /**
             * @brief      Generates SDF string for cylinder object
             *
             * @param[in]  model_name   The model name
             * @param[in]  mass         The mass
             * @param[in]  radius       The radius
             * @param[in]  length       The length
             * @param[in]  position     The position
             * @param[in]  orientation  The orientation
             *
             * @return     The cylinder SDF string
             */
            const std::string genCylinder(
                const std::string &model_name,
                const double mass,
                const double radius,
                const double length,
                const ignition::math::Vector3d position,
                const ignition::math::Quaterniond orientation);

            /**
             * @brief      Generates SDF string for box object
             *
             * @param[in]  model_name   The model name
             * @param[in]  mass         The mass
             * @param[in]  size         The box dimensions
             * @param[in]  position     The position
             * @param[in]  orientation  The orientation
             *
             * @return     The box SDF string
             */
            const std::string genBox(
                const std::string &model_name,
                const double mass,
                const ignition::math::Vector3d size,
                const ignition::math::Vector3d position,
                const ignition::math::Quaterniond orientation);
    };
}