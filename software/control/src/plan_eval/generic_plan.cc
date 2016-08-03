#include "generic_plan.h"
#include "drake/util/yaml/yamlUtil.h"

void GenericPlan::LoadConfigurationFromYAML(const std::string &name) {
  YAML::Node config = YAML::LoadFile(name);
  rpc_ = parseKinematicTreeMetadata(config["kinematic_tree_metadata"], robot_);

  std::map<Side, std::string> side_identifiers = {{Side::LEFT, "l"}, {Side::RIGHT, "r"}};
  for (const auto& side : Side::values)
    rpc_.hand_ids[side] = robot_.findLinkId(config["kinematic_tree_metadata"]["body_names"]["hands"][side_identifiers[side]].as<std::string>());
  rpc_.pelvis_id = robot_.findLinkId(config["kinematic_tree_metadata"]["body_names"]["pelvis"].as<std::string>());

  for (auto it = rpc_.foot_ids.begin(); it != rpc_.foot_ids.end(); it++) {
    std::cout << it->first.toString() << " foot name: " << robot_.getBodyOrFrameName(it->second) << std::endl;
  }

  for (auto it = rpc_.position_indices.legs.begin(); it != rpc_.position_indices.legs.end(); it++) {
    std::cout << it->first.toString() << " leg joints: ";
    for (size_t i = 0; i < it->second.size(); i++)
      std::cout << robot_.getPositionName(it->second[i]) << " ";
    std::cout << std::endl;
  }

  for (auto it = rpc_.position_indices.arms.begin(); it != rpc_.position_indices.arms.end(); it++) {
    std::cout << it->first.toString() << " arm joints: ";
    for (size_t i = 0; i < it->second.size(); i++)
      std::cout << robot_.getPositionName(it->second[i]) << " ";
    std::cout << std::endl;
  }

  // contacts
  std::string xyz[3] = {"x", "y", "z"};
  std::string corners[4] = {"left_toe", "right_toe", "left_heel", "right_heel"};
  for (auto it = rpc_.foot_ids.begin(); it != rpc_.foot_ids.end(); it++) {
    std::string foot_name = robot_.getBodyOrFrameName(it->second);
    Eigen::Matrix3Xd contacts(3, 4);
    for (int n = 0; n < 4; n++) {
      for (int i = 0; i < 3; i++)
        contacts(i, n) = config["foot_contact_offsets"][foot_name][corners[n]][xyz[i]].as<double>();
    }

    contact_offsets[it->second] = contacts;
    std::cout << foot_name << " contacts:\n" << contacts << std::endl;
  }

  // other params
  p_mu_ = get(config, "mu").as<double>();
  p_zmp_height_ = get(config, "zmp_height").as<double>();
  p_initial_transition_time_ = get(config, "initial_transition_time").as<double>();
  std::cout << "p_zmp_height: " << p_zmp_height_ << std::endl;
  std::cout << "mu: " << p_mu_ << std::endl;
  std::cout << "initial_transition_time: " << p_initial_transition_time_ << std::endl;
}

void GenericPlan::MakeSupportState(ContactState cs)
{
  std::vector<int> support_idx;
  switch (cs) {
    case DSc:
      support_idx.push_back(rpc_.foot_ids[Side::LEFT]);
      support_idx.push_back(rpc_.foot_ids[Side::RIGHT]);
      break;
    
    case SSL:
      support_idx.push_back(rpc_.foot_ids[Side::LEFT]);
      break;

    case SSR:
      support_idx.push_back(rpc_.foot_ids[Side::RIGHT]);
      break;

    default:
      std::cerr << "not a valid contact state\n";
      exit(-1);
  }
  support_state_.resize(support_idx.size());

  for (size_t s = 0; s < support_idx.size(); s++) {
    RigidBodySupportStateElement &support = support_state_[s];
    support.body = support_idx[s];
    support.use_contact_surface = true;
    support.support_surface = Eigen::Vector4d(0, 0, 1, 0);

    support.contact_points = contact_offsets.at(support_idx[s]);
  }
}

// utils
std::string PrimaryBodyOrFrameName(const std::string &full_body_name) {
  int i;
  for (i = 0; i < full_body_name.size(); i++) {
    if (std::strncmp(&full_body_name[i], "+", 1) == 0) {
      break;
    }
  }

  return full_body_name.substr(0, i);
}

void MakeDefaultBodyMotionData(BodyMotionData &body_motion, size_t num_T)
{
  body_motion.toe_off_allowed.resize(num_T, false);
  body_motion.in_floating_base_nullspace.resize(num_T, false);
  body_motion.control_pose_when_in_contact.resize(num_T, false);
  body_motion.transform_task_to_world = Eigen::Isometry3d::Identity();
  body_motion.xyz_proportional_gain_multiplier =
    Eigen::Vector3d::Constant(1);
  body_motion.xyz_damping_ratio_multiplier =
    Eigen::Vector3d::Constant(1);
  body_motion.exponential_map_proportional_gain_multiplier = 1;
  body_motion.exponential_map_damping_ratio_multiplier = 1;
  body_motion.weight_multiplier = Eigen::Vector6d::Constant(1); 
}

Eigen::Matrix<double,7,1> Isometry3dToVector7d(const Eigen::Isometry3d &pose)
{
  Eigen::Matrix<double,7,1> ret;
  ret.segment<3>(0) = pose.translation();
  ret.segment<4>(3) = rotmat2quat(pose.linear());
  return ret;
}

void KeyframeToState(const bot_core::robot_state_t &keyframe,
                     Eigen::VectorXd &q, Eigen::VectorXd &v) {
  q.resize(6 + keyframe.joint_position.size());
  v.resize(6 + keyframe.joint_velocity.size());
  // assuming floating base
  q[0] = keyframe.pose.translation.x;
  q[1] = keyframe.pose.translation.y;
  q[2] = keyframe.pose.translation.z;

  Eigen::Matrix<double, 4, 1> quat;
  quat[0] = keyframe.pose.rotation.w;
  quat[1] = keyframe.pose.rotation.x;
  quat[2] = keyframe.pose.rotation.y;
  quat[3] = keyframe.pose.rotation.z;
  q.segment<3>(3) = quat2rpy(quat);

  v[0] = keyframe.twist.linear_velocity.x;
  v[1] = keyframe.twist.linear_velocity.y;
  v[2] = keyframe.twist.linear_velocity.z;
  v[3] = keyframe.twist.angular_velocity.x;
  v[4] = keyframe.twist.angular_velocity.y;
  v[5] = keyframe.twist.angular_velocity.z;

  for (size_t i = 0; i < keyframe.joint_position.size(); i++) {
    q[i + 6] = keyframe.joint_position[i];
  }
  for (size_t i = 0; i < keyframe.joint_velocity.size(); i++) {
    v[i + 6] = keyframe.joint_velocity[i];
  }
}

Eigen::Vector6d getTaskSpaceVel(const RigidBodyTree &r,
                                const KinematicsCache<double> &cache,
                                int body_or_frame_id,
                                const Eigen::Vector3d &local_offset) {
  Eigen::Isometry3d H_body_to_frame;
  int body_idx = r.parseBodyOrFrameID(body_or_frame_id, &H_body_to_frame);

  const auto &element = cache.getElement(*(r.bodies[body_idx]));
  Eigen::Vector6d T = element.twist_in_world;
  Eigen::Vector3d pt = element.transform_to_world.translation();

  // get the body's task space vel
  Eigen::Vector6d v = T;
  v.tail<3>() += v.head<3>().cross(pt);

  // global offset between pt and body
  auto H_world_to_frame = element.transform_to_world * H_body_to_frame;
  Eigen::Isometry3d H_frame_to_pt(Eigen::Isometry3d::Identity());
  H_frame_to_pt.translation() = local_offset;
  auto H_world_to_pt = H_world_to_frame * H_frame_to_pt;
  Eigen::Vector3d world_offset =
      H_world_to_pt.translation() - element.transform_to_world.translation();

  // add the linear vel from the body rotation
  v.tail<3>() += v.head<3>().cross(world_offset);

  return v;
}

Eigen::MatrixXd getTaskSpaceJacobian(const RigidBodyTree &r,
                                     const KinematicsCache<double> &cache,
                                     int body,
                                     const Eigen::Vector3d &local_offset) {
  std::vector<int> v_or_q_indices;
  KinematicPath body_path = r.findKinematicPath(0, body);
  Eigen::MatrixXd Jg =
      r.geometricJacobian(cache, 0, body, 0, true, &v_or_q_indices);
  Eigen::MatrixXd J(6, r.num_velocities);
  // MatrixXd J(6, r.number_of_velocities());
  J.setZero();

  Eigen::Vector3d points = r.transformPoints(cache, local_offset, body, 0);

  int col = 0;
  for (std::vector<int>::iterator it = v_or_q_indices.begin();
       it != v_or_q_indices.end(); ++it) {
    // angular
    J.template block<SPACE_DIMENSION, 1>(0, *it) = Jg.block<3, 1>(0, col);
    // linear, just like the linear velocity, assume qd = 1, the column is the
    // linear velocity.
    J.template block<SPACE_DIMENSION, 1>(3, *it) = Jg.block<3, 1>(3, col);
    J.template block<SPACE_DIMENSION, 1>(3, *it).noalias() +=
        Jg.block<3, 1>(0, col).cross(points);
    col++;
  }

  return J;
}

Eigen::Vector6d getTaskSpaceJacobianDotTimesV(
    const RigidBodyTree &r, const KinematicsCache<double> &cache,
    int body_or_frame_id, const Eigen::Vector3d &local_offset) {
  // position of point in world
  Eigen::Vector3d p =
      r.transformPoints(cache, local_offset, body_or_frame_id, 0);
  Eigen::Vector6d twist = r.relativeTwist(cache, 0, body_or_frame_id, 0);
  Eigen::Vector6d J_geometric_dot_times_v =
      r.geometricJacobianDotTimesV(cache, 0, body_or_frame_id, 0);

  // linear vel of r
  Eigen::Vector3d pdot = twist.head<3>().cross(p) + twist.tail<3>();

  // each column of J_task Jt = [Jg_omega; Jg_v + Jg_omega.cross(p)]
  // Jt * v, angular part stays the same,
  // linear part = [\dot{Jg_v}v + \dot{Jg_omega}.cross(p) +
  // Jg_omega.cross(rdot)] * v
  //             = [lin of JgdotV + ang of JgdotV.cross(p) + omega.cross(rdot)]
  Eigen::Vector6d Jdv = J_geometric_dot_times_v;
  Jdv.tail<3>() +=
      twist.head<3>().cross(pdot) + J_geometric_dot_times_v.head<3>().cross(p);

  return Jdv;
}
 
