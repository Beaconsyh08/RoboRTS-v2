#include <ros/ros.h>
#include <goal_factory.h>

int main(int argc, char **argv){
    ros::init(argc, argv, "goal_factory_node");
    ros::NodeHandle nh;
    Goal_Factory goal_factory(nh);
    ROS_INFO("start decision node");
    goal_factory.run();
}