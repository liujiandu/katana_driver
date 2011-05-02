/*
 * based on teleop_pr2_keyboard
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Author: Kevin Watts

//TODO adjust author and licence, specific format needed?

#include <katana_teleop/katana_teleop_key.h>

namespace katana
{

KatanaTeleopKey::KatanaTeleopKey() :
  action_client("joint_movement_action", true)
{

  ROS_INFO("KatanaTeleopKey starting...");
  ros::NodeHandle n_;
  ros::NodeHandle n_private("~");

  n_.param("increment", increment, 0.017453293); // default increment = 1°
  n_.param("increment_step", increment_step, 0.017453293); // default step_increment = 1°
  n_.param("increment_step_scaling", increment_step_scaling, 1.0); // default scaling = 1

  js_sub_ = n_.subscribe("joint_states", 1000, &KatanaTeleopKey::jointStateCallback, this);

  sensor_msgs::JointState jg;
  set_initial = false;

  jointIndex = 0;

  action_client.waitForServer();

  // Gets all of the joints
  XmlRpc::XmlRpcValue joint_names;

  // Gets all of the joints
  if (!n_.getParam("katana_joints", joint_names))
  {
    ROS_ERROR("No joints given. (namespace: %s)", n_.getNamespace().c_str());
  }
  joint_names_.resize(joint_names.size());

  if (joint_names.getType() != XmlRpc::XmlRpcValue::TypeArray)
  {
    ROS_ERROR("Malformed joint specification.  (namespace: %s)", n_.getNamespace().c_str());
  }

  for (size_t i = 0; i < joint_names.size(); ++i)
  {

    XmlRpc::XmlRpcValue &name_value = joint_names[i];

    if (name_value.getType() != XmlRpc::XmlRpcValue::TypeString)
    {
      ROS_ERROR("Array of joint names should contain all strings.  (namespace: %s)",
          n_.getNamespace().c_str());
    }

    joint_names_[i] = (std::string)name_value;

  }

  // Gets all of the gripper joints
  XmlRpc::XmlRpcValue gripper_joint_names;

  // Gets all of the joints
  if (!n_.getParam("katana_gripper_joints", gripper_joint_names))
  {
    ROS_ERROR("No gripper joints given. (namespace: %s)", n_.getNamespace().c_str());
  }

  gripper_joint_names_.resize(gripper_joint_names.size());

  if (gripper_joint_names.getType() != XmlRpc::XmlRpcValue::TypeArray)
  {
    ROS_ERROR("Malformed gripper joint specification.  (namespace: %s)", n_.getNamespace().c_str());
  }
  for (size_t i = 0; i < gripper_joint_names.size(); ++i)
  {
    XmlRpc::XmlRpcValue &name_value = gripper_joint_names[i];
    if (name_value.getType() != XmlRpc::XmlRpcValue::TypeString)
    {
      ROS_ERROR("Array of gripper joint names should contain all strings.  (namespace: %s)",
          n_.getNamespace().c_str());
    }

    gripper_joint_names_[i] = (std::string)name_value;
  }

  combined_joints_.resize(joint_names_.size() + gripper_joint_names_.size());

  for (unsigned int i = 0; i < joint_names_.size(); i++)
  {
    combined_joints_[i] = joint_names_[i];
  }

  for (unsigned int i = 0; i < gripper_joint_names_.size(); i++)
  {
    combined_joints_[joint_names_.size() + i] = gripper_joint_names_[i];
  }

  giveInfo();

}

void KatanaTeleopKey::giveInfo()
{
  ros::spinOnce();

  ROS_INFO("---------------------------");
  ROS_INFO("Use 'WS' to increase/decrease the joint position about one increment");
  ROS_INFO("Current increment is set to: %f", increment);
  ROS_INFO("Use '+#' to alter the increment by a increment/decrement of: %f", increment_step);
  ROS_INFO("Use ',.' to alter the increment_step_size altering the scaling factor by -/+ 1.0");
  ROS_INFO("Current scaling is set to: %f" , increment_step_scaling);
  ROS_INFO("---------------------------");
  ROS_INFO("Use 'R' to return to the arm's initial pose");
  ROS_INFO("---------------------------");
  ROS_INFO("Use 'AD' to switch to the next/previous joint");

  for (unsigned int i = 0; i < joint_names_.size(); i++)
  {
    ROS_INFO("Use '%d' to switch to Joint: '%s'",i, joint_names_[i].c_str());
  }

  for (unsigned int i = 0; i < gripper_joint_names_.size(); i++)
  {
    ROS_INFO("Use '%d' to switch to Gripper Joint: '%s'",i + joint_names_.size(), gripper_joint_names_[i].c_str());
  }

  if (!current_pose_.name.empty())
  {
    ROS_INFO("---------------------------");
    ROS_INFO("Current Joint Positions:");

    for (unsigned int i = 0; i < current_pose_.position.size(); i++)
    {
      ROS_INFO("Joint %d - %s: %f", i, current_pose_.name[i].c_str(), current_pose_.position[i]);
    }
  }
}

void KatanaTeleopKey::jointStateCallback(const sensor_msgs::JointState::ConstPtr& js)
{

  ROS_INFO("KatanaTeleopKeyboard received a new JointState");

  current_pose_.name = js->name;
  current_pose_.position = js->position;

  if (!set_initial)
  {
    ROS_INFO("KatanaTeleopKeyboard received initial JointState");
    initial_pose_.name = js->name;
    initial_pose_.position = js->position;
    set_initial = true;
  }
}

bool KatanaTeleopKey::matchJointGoalRequest(double increment)
{

  bool found_match = false;

  for (unsigned int i = 0; i < current_pose_.name.size(); i++)
  {

    if (current_pose_.name[i] == combined_joints_[jointIndex])
    {

      ROS_WARN("incoming inc: %f - curren_pose: %f - resulting pose: %f ",increment, current_pose_.position[i], current_pose_.position[i] + increment);
      movement_goal_.position.push_back(current_pose_.position[i] + increment);
      found_match = true;
      break;

    }
  }

  return found_match;
}

void KatanaTeleopKey::keyboardLoop()
{

  char c;
  bool dirty = true;
  bool shutdown = false;

  // get the console in raw mode
  tcgetattr(kfd, &cooked);
  memcpy(&raw, &cooked, sizeof(struct termios));
  raw.c_lflag &= ~(ICANON | ECHO);
  // Setting a new line, then end of file
  raw.c_cc[VEOL] = 1;
  raw.c_cc[VEOF] = 2;
  tcsetattr(kfd, TCSANOW, &raw);

  while (!shutdown)
  {

    if (set_initial)
    {
      // get the next event from the keyboard
      if (read(kfd, &c, 1) < 0)
      {
        perror("read():");
        exit(-1);
      }

      switch (c)
      {
        // Increasing/Decreasing JointPosition
        case KEYCODE_W:

          if (matchJointGoalRequest(increment))
          {
            movement_goal_.name.push_back(combined_joints_[jointIndex]);
          }
          else
          {
            ROS_WARN("movement with the desired joint: %s failed due to a mismatch with the current joint state", combined_joints_[jointIndex].c_str());
            dirty = false;
            break;
          }

          dirty = true;
          break;

        case KEYCODE_S:

          if (matchJointGoalRequest(-increment))
          {
            movement_goal_.name.push_back(combined_joints_[jointIndex]);
          }
          else
          {
            ROS_WARN("movement with the desired joint: %s failed due to a mismatch with the current joint state", combined_joints_[jointIndex].c_str());
            dirty = false;
            break;
          }

          dirty = true;
          break;

          // Switching active Joint
        case KEYCODE_D:

          // use this line if you want to use "the gripper" instead of the single gripper joints
          jointIndex = (jointIndex + 1) % (joint_names_.size() + 1);

          // use this line if you want to select specific gripper joints
          //jointIndex = (jointIndex + 1) % combined_joints_.size();

          dirty = false;
          break;

        case KEYCODE_A:

          // use this line if you want to use "the gripper" instead of the single gripper joints
          jointIndex = (jointIndex - 1) % (joint_names_.size() + 1);

          // use this line if you want to select specific gripper joints
          //jointIndex = (jointIndex - 1) % combined_joints_.size();

          dirty = false;
          break;

        case KEYCODE_R:
          ROS_INFO("resetting arm to it's initial pose..");

          movement_goal_.name = initial_pose_.name;
          movement_goal_.position = initial_pose_.position;
          dirty = true;
          break;

        case KEYCODE_Q:
          // in case fo shutting down the teleop node the arm is moved back into it's initial pose
          // assuming that this is a proper resting pose for the arm

          ROS_INFO("Shutting down Katana Tele Operation");
          movement_goal_.name = initial_pose_.name;
          movement_goal_.position = initial_pose_.position;
          dirty = true;
          shutdown = true;
          break;

        case KEYCODE_I:

          giveInfo();
          dirty = false;
          break;

        case KEYCODE_0:

          if (combined_joints_.size() > 0)
          {

            ROS_DEBUG("You choose to adress joint no. 0: %s", combined_joints_[0].c_str());
            jointIndex = 0;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 0 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_1:

          if (combined_joints_.size() > 1)
          {

            ROS_DEBUG("You choose to adress joint no. 1: %s", combined_joints_[1].c_str());
            jointIndex = 1;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 1 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_2:

          if (combined_joints_.size() > 2)
          {

            ROS_DEBUG("You choose to adress joint no. 2: %s", combined_joints_[2].c_str());
            jointIndex = 2;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 2 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_3:

          if (combined_joints_.size() > 3)
          {

            ROS_DEBUG("You choose to adress joint no. 3: %s", combined_joints_[3].c_str());
            jointIndex = 3;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 3 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_4:

          if (combined_joints_.size() > 4)
          {

            ROS_DEBUG("You choose to adress joint no. 4: %s", combined_joints_[4].c_str());
            jointIndex = 4;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 4 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_5:

          if (combined_joints_.size() > 5)
          {

            ROS_DEBUG("You choose to adress joint no. 5: %s", combined_joints_[5].c_str());
            jointIndex = 5;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 5 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_6:

          if (combined_joints_.size() > 6)
          {

            ROS_DEBUG("You choose to adress joint no. 6: %s", combined_joints_[6].c_str());
            jointIndex = 6;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 6 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_7:

          if (combined_joints_.size() > 7)
          {

            ROS_DEBUG("You choose to adress joint no. 7: %s", combined_joints_[7].c_str());
            jointIndex = 7;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 7 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_8:

          if (combined_joints_.size() > 8)
          {

            ROS_DEBUG("You choose to adress joint no. 8: %s", combined_joints_[8].c_str());
            jointIndex = 8;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 8 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_9:

          if (combined_joints_.size() > 9)
          {

            ROS_DEBUG("You choose to adress joint no. 9: %s", combined_joints_[9].c_str());
            jointIndex = 9;
            dirty = false;
            break;

          }
          else
          {

            ROS_WARN("Joint Index No. 9 can not be adressed!");
            dirty = false;
            break;
          }

        case KEYCODE_PLUS:
          increment += (increment_step * increment_step_scaling);
          ROS_DEBUG("Increment increased to: %f",increment);
          dirty = false;
          break;

        case KEYCODE_NUMBER:

          increment -= (increment_step * increment_step_scaling);
          if(increment < 0){
            increment = 0.0;
          }
          ROS_DEBUG("Increment decreased to: %f",increment);
          dirty = false;
          break;

        case KEYCODE_POINT:

          increment_step_scaling += 1.0;
          ROS_DEBUG("Increment_Scaling increased to: %f",increment_step_scaling);
          dirty = false;
          break;

        case KEYCODE_COMMA:

          increment_step_scaling -= 1.0;
          ROS_DEBUG("Increment_Scaling decreased to: %f",increment_step_scaling);
          dirty = false;
          break;

      } // end switch case

      katana::JointMovementGoal goal;

      goal.jointGoal = movement_goal_;

      if (dirty == true)
      {
        ROS_WARN("Sending new JointMovementActionGoal..");

        for (size_t i = 0; i < goal.jointGoal.name.size(); i++)
        {

          ROS_INFO("Joint: %s to %f rad", goal.jointGoal.name[i].c_str(), goal.jointGoal.position[i]);

        }


        bool finished_within_time = false;
        action_client.sendGoal(goal);
        finished_within_time = action_client.waitForResult(ros::Duration(1.0));
        if (!finished_within_time)
        {
           action_client.cancelGoal();
           ROS_INFO("Timed out achieving goal!");
        }
        else
        {
           actionlib::SimpleClientGoalState state = action_client.getState();
           bool success = (state == actionlib::SimpleClientGoalState::SUCCEEDED);
           if (success)
             ROS_INFO("Action finished: %s",state.toString().c_str());
           else
             ROS_INFO("Action failed: %s", state.toString().c_str());

           ROS_INFO("...goal was successfully send!");
           giveInfo();
        }

        movement_goal_.name.clear();
        movement_goal_.position.clear();


        dirty = false;
      }

    } // if set initial end

   // ROS_INFO("Listen to joint_state");jointIndex
   // ros::spinOnce();
  }
}
}// end namespace "katana"

void quit(int sig)
{
  tcsetattr(kfd, TCSANOW, &cooked);
  exit(0);
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "katana_teleop_key");

  katana::KatanaTeleopKey ktk;

  signal(SIGINT, quit);

  ktk.keyboardLoop();

  return (0);
}

