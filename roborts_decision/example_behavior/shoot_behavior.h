#ifndef ROBORTS_DECISION_SHOOTBEHAVIOR_H
#define ROBORTS_DECISION_SHOOTBEHAVIOR_H

#include <vector>
#include <string>
#include <cmath>

#include "io/io.h"

#include "../blackboard/blackboard.h"
#include "../executor/chassis_executor.h"
#include "../behavior_tree/behavior_state.h"
#include "../proto/decision.pb.h"

#include <ros/ros.h>

#include "roborts_sim/CheckBullet.h"
#include "roborts_sim/ReloadCmd.h"
#include "roborts_sim/ShootCmd.h"

#include "roborts_msgs/RobotHeat.h"

namespace roborts_decision {

const int PROJECTILE_SPEED = 25;
const int BARREL_HEAT_LIMIT = 360;
const int BARREL_HEAT_UPPERBOUND = 720;

class ShootBehavior {

public:
  ShootBehavior(ChassisExecutor* &chassis_executor,
                 Blackboard* &blackboard,
                 const std::string & proto_file_path) : chassis_executor_(chassis_executor),
                                                        blackboard_(blackboard) {

    // init whirl velocity
    whirl_vel_.linear.x = 0;
    whirl_vel_.linear.y = 0;
    whirl_vel_.linear.z = 0;

    whirl_vel_.angular.x = 0;
    whirl_vel_.angular.y = 0;
    whirl_vel_.angular.z = 0;

    // Load Param from config file (Current: ../config/decision.prototxt)
    if (!LoadParam(proto_file_path)) {
      ROS_ERROR("%s can't open file", __FUNCTION__);
    }

    // Get self Robot ID
    std::string ns = ros::this_node::getNamespace();
    if (ns == "r1") {
      robot_ = 1;
      enemy_ = 3;
    } else if (ns == "r3") {
      robot_ = 3;
      enemy_ = 1;
    } else {
      ROS_WARN("Error happens when checking self Robot ID, namely %s, in function %s", ns.c_str(), __FUNCTION__);
    }

    // Service Client Register
    check_bullet_client_ = nh_.serviceClient<roborts_sim::CheckBullet>("/check_bullet");
    shoot_client_ = nh_.serviceClient<roborts_sim::ShootCmd>("/shoot");

    // Topic Subscriber Register
    subs_.push_back(nh_.subscribe<roborts_msgs::RobotHeat>("robot_heat", 30, &ShootBehavior::BarrelHeatCallback, this));

  }

  // TODO: I'm wondering is there a good way to let our robot shoot in a more advantageous way, like behind a barricade.
  void Run() {
    if (blackboard_->IsEnemyDetected()) {
      if (!HasBullet()) {
        ROS_WARN("I have no ammo, %s", __FUNCTION__);
        chassis_executor_->Execute(rot_whirl_vel_);
        return;
      } else {
        // If robot plans to shoot, better face to the enemy
        // Get robot and enemy position under map frame
        geometry_msgs::PoseStamped enemy_map_pose = blackboard_->GetEnemy();
        geometry_msgs::PoseStamped robot_map_pose = blackboard_->GetRobotMapPose();
        // Let our robot directly faces to enemy
        float dx = enemy_map_pose.pose.position.x - robot_map_pose.pose.position.x;
        float dy = enemy_map_pose.pose.position.y - robot_map_pose.pose.position.y;
        float yaw = static_cast<float>(std::atan2(dy,dx));
        auto quaternion = tf::createQuaternionMsgFromRollPitchYaw(0,0,yaw);

        geometry_msgs::PoseStamped shoot_pose;
        shoot_pose.header.frame_id = "map";
        shoot_pose.header.stamp = ros::Time::now();
        shoot_pose.pose.position.x = robot_map_pose.pose.position.x;
        shoot_pose.pose.position.y = robot_map_pose.pose.position.y;
        shoot_pose.pose.orientation = quaternion;
        chassis_executor_->Execute(shoot_pose);

        if (barrel_heat_ >= BARREL_HEAT_LIMIT - PROJECTILE_SPEED) {
          ROS_INFO("In current mode, robot's barrel heat won't exceed heat limit.");
          return;
        } else {
          ShootEnemy();
          return;
        }
      }
    } else {
      ROS_INFO("Decided to shoot but enemy is not detected, rotate to find enemy. %s", __FUNCTION__);
      chassis_executor_->Execute(rot_whirl_vel_);
    }
  }

  void Cancel() {
    chassis_executor_->Cancel();
  }

  BehaviorState Update() {
    return chassis_executor_->Update();
  }

  bool LoadParam(const std::string &proto_file_path) {
    roborts_decision::DecisionConfig decision_config;
    if (!roborts_common::ReadProtoFromTextFile(proto_file_path, &decision_config)) {
      return false;
    }
    // Add Loading statements here
    // The default whirl behavior is rotating in counter-clock direction
    whirl_vel_.angular.z = decision_config.whirl_vel().angle_z_vel();
    whirl_vel_.angular.y = decision_config.whirl_vel().angle_y_vel();
    whirl_vel_.angular.x = decision_config.whirl_vel().angle_x_vel();
    rot_whirl_vel_.angular.z = decision_config.whirl_vel().angle_z_vel();
    rot_whirl_vel_.angular.y = decision_config.whirl_vel().angle_y_vel();
    rot_whirl_vel_.angular.x = decision_config.whirl_vel().angle_x_vel();
    return true;
  }

  ~ShootBehavior() {}

private:
  bool HasBullet() {
    roborts_sim::CheckBullet check_bullet_srv;
    check_bullet_srv.request.robot_id = robot_;
    if (check_bullet_client_.call(check_bullet_srv)) {
      return (check_bullet_srv.response.remaining_bullet != 0);
    } else {
      ROS_ERROR("Failed to call service checkBullet!");
      return false;
    }
  }

  void ShootEnemy() {
    roborts_sim::ShootCmd shoot_srv;
    shoot_srv.request.robot = robot_;
    shoot_srv.request.enemy = enemy_;
    if (shoot_client_.call(shoot_srv)) {
      ROS_INFO("Robot %d attempted to shoot Robot %d", robot_, enemy_);
    } else {
      ROS_ERROR("Failed to call service Shoot!");
    }
  }

  void BarrelHeatCallback(const roborts_msgs::RobotHeat::ConstPtr &robot_heat) {
    barrel_heat_ = robot_heat->shooter_heat;
  }

private:
  //! executor
  ChassisExecutor* const chassis_executor_;

  //! perception information
  Blackboard* const blackboard_;

  //! Node Handle
  ros::NodeHandle nh_;

  //! Service Clients
  ros::ServiceClient check_bullet_client_;
  ros::ServiceClient shoot_client_;

  //! Topic Subscribers
  std::vector <ros::Subscriber> subs_;

  //! ID for current robot
  int robot_;
  int enemy_;

  //! Barrel Heat
  int barrel_heat_;

  //! Control whirl velocity
  geometry_msgs::Twist whirl_vel_;

  //! Rotation whirl config message
  geometry_msgs::Twist rot_whirl_vel_;
};
}


#endif //ROBORTS_DECISION_SHOOTBEHAVIOR_H
