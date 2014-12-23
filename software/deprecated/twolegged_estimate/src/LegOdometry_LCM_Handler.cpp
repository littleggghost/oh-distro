
/*
 * Refer to "LegOdometry_LCM_Handler.h" for comments on the purpose of each function and member, comments here in the .cpp relate
 * to more development issues and specifics. The user only need worry about the definitions and descriptions given in the header file,
 * assuming a good software design was done.
 * d fourie
 * 3/24/2013
*/

#include <iostream>
#include <exception>
//#include <stdio.h>
//#include <inttypes.h>



#include <leg-odometry/LegOdometry_LCM_Handler.hpp>
#include <leg-odometry/QuaternionLib.h>
#include <leg-odometry/data_fusion_parameters.hpp>

using namespace TwoLegs;
using namespace std;

LegOdometry_Handler::LegOdometry_Handler(boost::shared_ptr<lcm::LCM> &lcm_, command_switches* commands):
        _finish(false), lcm_(lcm_), inert_odo(0.001) {
  // Create the object we want to use to estimate the robot's pelvis position
  // In this case its a two legged vehicle and we use TwoLegOdometry class for this task
  
  _switches = commands;
  filter_joints_vector_size_set = false;
  
  std::cout << "Switches value for listen to LCM trues is: " << _switches->lcm_read_trues << std::endl;
  
  // There used to be polymorphism here, but that soldier was abandoned for an easier and more cumbersome maneuver
  //for (int i=0;i<FILTER_ARR;i++) {_filter[i] = &lpfilter[i];}
  
   _botparam = bot_param_new_from_server(lcm_->getUnderlyingLCM(), 0);
   _botframes= bot_frames_get_global(lcm_->getUnderlyingLCM(), _botparam);
  
  FovisEst.P << NAN,NAN,NAN;
  FovisEst.V << 0.,0.,0.;

  body_to_head.setIdentity();

  slide_err_at_step.setZero();

  first_get_transforms = true;
  zvu_flag = false;
  biasmessagesent = false;
  ratecounter = 0;
  local_to_head_vel_diff.setSize(3);
  local_to_head_acc_diff.setSize(3);
  local_to_head_rate_diff.setSize(3);
  zvu_timetrigger.setParameters(0.4,0.6,40000,40000);
  
  stageA_test_vel.setSize(3);

  rate_changer.setSize(3);
  publishvelocitiesvec.setZero();

  fusion_rate.setSize(1);
  bias_publishing_rate.setSize(1);
  estimating_biases = 0;

  acc_bias_est.setSize(3);

  persistlegchangeflag = false;

  // let the biases be averaged and estimated during the first portion of the task
  allowbiasestimation = false; // Do not include initial trasients to the bias averaging process
  acc_bias_avg.setSize(3);
  trigger_bias_averaging.setParameters(0.003,0.01,1000000,1000000);
  trigger_bias_averaging.forceHigh(); // Only trip low when the bias update values have reduced to acceptable levels for averaging
  expire_bias_avg_process.setDesiredPeriod_us(10000000);

  // Publish EST_ROBOT_STATE and other messages at 200 Hz
  rate_changer.setDesiredPeriod_us(0,3500);

  // Tuning parameters for data fusion
  fusion_rate.setDesiredPeriod_us(0,DATA_FUSION_PERIOD-500);

  bias_publishing_rate.setDesiredPeriod_us(0,BIAS_PUBLISH_PERIOD-500);
  df_feedback_gain = -0.5;
  df_events = 0;

  checkforzero = 0;

#ifdef DO_FOOT_SLIP_FEEDBACK
  {
    // TODO -- remove this -- used to subjectively measure the foot velocity
    //Eigen::VectorXd w(5);
    //w << 0.2,0.2,0.2,0.2,0.2;
    //Eigen::VectorXd t(5);
    //t << 5000, 10000, 15000, 20000, 25000;
    //SFootPrintOut.setSize(3);
    //FootVelCompensation.setSize(3);
  }
#endif


  imu_msg_received = false;

  for (int i=0;i<3;i++) {
    median_filter[i].setLength(_switches->medianlength);
  }

#ifdef LOG_LEG_TRANSFORMS
  for (int i=0;i<4;i++) {
    // left vel, right vel, left rate, right rate
    pelvis_to_feet_speed[i].setSize(3);
  }
  for (int i=0;i<12;i++) {
    pelvis_to_feet_transform[i]=0.; // left vel, right vel, left rate, right rate
  }
#endif

  if (_switches->lcm_add_ext) {
    _channel_extension = "";
  } else {
    _channel_extension = "_VO";
  }
  
  if(!lcm_->good())
    return;
  
  fk_data.model_ = boost::shared_ptr<ModelClient>(new ModelClient(lcm_->getUnderlyingLCM(), 0));
  
#ifdef VERBOSE_DEGUG
  std::cout << "LegOdometry_handler is now subscribing to LCM messages: " << "TRUE_ROBOT_STATE, " << "TORSO_IMU" << std::endl; 
#endif
  
  lcm_->subscribe("TRUE_ROBOT_STATE",&LegOdometry_Handler::robot_state_handler,this);
  if (_switches->do_estimation) {
    lcm_->subscribe("TORSO_IMU",&LegOdometry_Handler::torso_imu_handler,this);
  }

  lcm_->subscribe("FOVIS_REL_ODOMETRY",&LegOdometry_Handler::delta_vo_handler,this);
  
  /*
  if (_switches->lcm_read_trues) {
    // now we can listen to the true values of POSE_HEAD
    lcm_->subscribe("POSE_HEAD_TRUE", &LegOdometry_Handler::pose_head_true_handler, this);
  }
  */
  
  // Parse KDL tree
    if (!kdl_parser::treeFromString(  fk_data.model_->getURDFString() , fk_data.tree)){
      std::cerr << "ERROR: Failed to extract kdl tree from xml robot description" << std::endl;
      return;
    }
    
    fk_data.fksolver_ = boost::shared_ptr<KDL::TreeFkSolverPosFull_recursive>(new KDL::TreeFkSolverPosFull_recursive(fk_data.tree));
    
  stillbusy = false;
  
  // This is for viewing results in the collections_viewer. check delete of new memory
  lcm_viewer = lcm_create(NULL);
  
  poseplotcounter = 0;
  collectionindex = 101;
  _obj = new ObjectCollection(1, std::string("Objects"), VS_OBJ_COLLECTION_T_POSE3D);
  _obj_leg_poses = new ObjectCollection(1, std::string("Objects"), VS_OBJ_COLLECTION_T_POSE3D);
  _link = new LinkCollection(2, std::string("Links"));
  
  firstpass = 1;//LowPassFilter::getTapSize(); the filter now initializes internally from the first sample -- only a single outside state from the user is required, i.e. transparent
  
  pulse_counter = 0;

  time_avg_counter = 0;
  elapsed_us = 0;
  maxtime = 0;
  prev_frame_utime = 0;

  _leg_odo = new TwoLegOdometry(_switches->log_data_files, _switches->publish_footcontact_states, _switches->mass);//900.f);
//#if defined( DISPLAY_FOOTSTEP_POSES ) || defined( DRAW_DEBUG_LEGTRANSFORM_POSES )
  if (_switches->draw_footsteps) {
  _viewer = new Viewer(lcm_viewer);
  }
  //#endif
    state_estimate_error_log.Open(_switches->log_data_files,"true_estimated_states.csv");
  joint_data_log.Open(_switches->log_data_files,"joint_data.csv");
  return;
}

LegOdometry_Handler::~LegOdometry_Handler() {
  
  state_estimate_error_log.Close();
  joint_data_log.Close();

  //delete model_;
  delete _leg_odo;
  delete _obj;
  delete _obj_leg_poses;
  delete _link;
  
  joint_lpfilters.clear();
  
  lcm_destroy(lcm_viewer); //destroy viewer memory at executable end
  delete _viewer;
  
  // Not sure if the pointers must be deleted here, this may try and delete a pointer to a static memory location -- therefore commented
  //delete _botframes;
  //delete _botparam;
  
  cout << "Everything Destroyed in LegOdometry_Handler::~LegOdometry_Handler()" << endl;
  return;
}

void LegOdometry_Handler::InitializeFilters(const int num_filters) {
  
  for (int i=0;i<num_filters;i++) {
    LowPassFilter member;
    joint_lpfilters.push_back(member);
  }
}

// To be moved to a better abstraction location
void LegOdometry_Handler::DetermineLegContactStates(long utime, float left_z, float right_z) {
  // The idea here is to determine the contact state of each foot independently
  // to enable better initialization logic when the robot is stating up, or stading up after falling down
  _leg_odo->foot_contact->updateSingleFootContactStates(utime, left_z, right_z);
}

void LegOdometry_Handler::FilterHandForces(const drc::robot_state_t* msg, drc::robot_state_t* estmsg) {

  estmsg->force_torque.l_hand_force[0] = lefthandforcesfilters[0].processSample( msg->force_torque.l_hand_force[0] );
  estmsg->force_torque.l_hand_force[1] = lefthandforcesfilters[1].processSample( msg->force_torque.l_hand_force[1] );
  estmsg->force_torque.l_hand_force[2] = lefthandforcesfilters[2].processSample( msg->force_torque.l_hand_force[2] );

  estmsg->force_torque.l_hand_torque[0] = lefthandforcesfilters[3].processSample( msg->force_torque.l_hand_torque[0] );
  estmsg->force_torque.l_hand_torque[1] = lefthandforcesfilters[4].processSample( msg->force_torque.l_hand_torque[1] );
  estmsg->force_torque.l_hand_torque[2] = lefthandforcesfilters[5].processSample( msg->force_torque.l_hand_torque[2] );

  estmsg->force_torque.r_hand_force[0] = righthandforcesfilters[0].processSample( msg->force_torque.r_hand_force[0] );
  estmsg->force_torque.r_hand_force[1] = righthandforcesfilters[1].processSample( msg->force_torque.r_hand_force[1] );
  estmsg->force_torque.r_hand_force[2] = righthandforcesfilters[2].processSample( msg->force_torque.r_hand_force[2] );

  estmsg->force_torque.r_hand_torque[0] = righthandforcesfilters[3].processSample( msg->force_torque.r_hand_torque[0] );
  estmsg->force_torque.r_hand_torque[1] = righthandforcesfilters[4].processSample( msg->force_torque.r_hand_torque[1] );
  estmsg->force_torque.r_hand_torque[2] = righthandforcesfilters[5].processSample( msg->force_torque.r_hand_torque[2] );
}

void LegOdometry_Handler::ParseFootForces(const drc::robot_state_t* msg, double &left_force, double &right_force) {
  
  left_force  = (double)lpfilter[0].processSample( msg->force_torque.l_foot_force_z );
  right_force = (double)lpfilter[1].processSample( msg->force_torque.r_foot_force_z );

}

InertialOdometry::DynamicState LegOdometry_Handler::data_fusion(  const unsigned long long &uts,
                                  const InertialOdometry::DynamicState &LeggO,
                                  const InertialOdometry::DynamicState &InerO,
                                  const InertialOdometry::DynamicState &Fovis) {


  InertialOdometry::DynamicState retstate;
  retstate = LeggO;

  Eigen::VectorXd dummy(1);
  dummy << 0;

  if (fusion_rate.genericRateChange(uts,dummy,dummy)) {
    // Yes now we do comparison and feedback

    df_events++;

    Eigen::Vector3d err_b, errv_b, errv_b_VO;
    Eigen::Matrix3d C_wb;


    // Reset inertial position offset here

    double a;
    double b;

    a = 1/(1 + 1000*err_b.norm());
    b = 1/(1 + 100*errv_b.norm());

    // test for zero velocity
    double speed;

    speed = LeggO.V.norm();

    C_wb = q2C(InerO.lQb).transpose();

    // Determine the error in the body frame
    err_b = C_wb * (LeggO.P - InerO.P);
    errv_b = C_wb * (LeggO.V - InerO.V);
    errv_b_VO = C_wb * (Fovis.V - InerO.V);


    // We should force velocity back to zero if we are confident that robot is standing still
    // Do that here
    //TODO


    double db_a[3];

    // ZERO VELOCITY UPDATE BLOCK
    if (true) {
      //std::cout << "double value is: " << uts << " | " << ((speed < ZU_SPEED_THRES)) << ", " << ((double)(speed < ZU_SPEED_THRES)) << std::endl;
      zvu_timetrigger.UpdateState(uts,((double)(speed < ZU_SPEED_THRES)));
      //std::cout << "ST state is: " << zvu_timetrigger.getState() << std::endl;

      for (int i=0;i<3;i++) {
        if (zvu_timetrigger.getState() >= 0.5 && !(speed > ZU_SPEED_THRES)) {
          // This is the zero velocity update step
          //std::cout << "ZVU is happening\n";
          zvu_flag = true;

          db_a[i] = - INS_POS_FEEDBACK_GAIN * err_b(i) - INS_VEL_FEEDBACK_GAIN * errv_b(i);

          feedback_loggings[i] = - INS_POS_FEEDBACK_GAIN * err_b(i);
          feedback_loggings[i+3] = - INS_VEL_FEEDBACK_GAIN * errv_b(i);
        }
        else
        {
          zvu_flag = false;

          db_a[i] = - 0 * INS_POS_FEEDBACK_GAIN * err_b(i) - 0 * INS_VEL_FEEDBACK_GAIN * errv_b(i);

          feedback_loggings[i] = - 0 * INS_POS_FEEDBACK_GAIN * err_b(i);
          feedback_loggings[i+3] = - 0 * INS_VEL_FEEDBACK_GAIN * errv_b(i);
        }

      }

    } else {
      for (int i=0;i<3;i++) {
        db_a[i] = - INS_POS_FEEDBACK_GAIN * err_b(i) - INS_VEL_FEEDBACK_GAIN * errv_b_VO(i);
      }
    }



    // Update the bias estimates

    // if not expiration timer
    //std::cout << "before " << expire_bias_avg_process.getState() << " | " << inert_odo.imu_compensator.get_accel_biases().transpose() << std::endl;
    if (!expire_bias_avg_process.getState()) {
      inert_odo.imu_compensator.AccumulateAccelBiases(Eigen::Vector3d(db_a[0], db_a[1], db_a[2]));

      double norm = sqrt(db_a[0]*db_a[0]+db_a[1]*db_a[1]+db_a[2]*db_a[2]);

      if (speed > ZU_SPEED_THRES) {
        // reset the trigger
        trigger_bias_averaging.Reset();
        trigger_bias_averaging.forceHigh();
        //std::cout << "reseting trigger_bias_averaging ST" <<std::endl;
      }

      trigger_bias_averaging.UpdateState(uts,norm);
      //std::cout << "triggerstate: " << trigger_bias_averaging.getState() << " | " << norm  << " | " << !(speed > ZU_SPEED_THRES) << std::endl;

      //determine whether we can start to average the bias estimate
      if (trigger_bias_averaging.getState() <= 0.5) {
        // new we can start the averaging process
        //std:: cout << "Triggering the start of bias estimation at uts: " << uts << std::endl;
        allowbiasestimation = true;
        // We should used this same logic to restrict the initial conditions for the velocity states - -by slaing to the leg odoemtry during th initialization phase
        //retstate.V = LeggO.V;
      }
    }

    if (allowbiasestimation) {
      //std::cout << "Bias averaging is being allowed.\n";

      acc_bias_avg.processSamples(inert_odo.imu_compensator.getAccelBiases());
      //std::cout << "bias is: " << acc_bias_avg.getCA().transpose() << std::endl;

      // we should run run the expire counter here
      //std::cout << "expirecounter: " << expire_bias_avg_process.getRemainingTime_us() << std::endl;

      if (expire_bias_avg_process.processSample(uts)) {
        // bias averaging period has expired
        allowbiasestimation = false;

        double avgbiases[3];
        Eigen::Vector3d cabias;

        cabias = acc_bias_avg.getCA();

        for (int i=0;i<3;i++) {
          avgbiases[i] = cabias(i);
        }

        //std::cout << "SettingAccelBiases on timer expire to: " << cabias.transpose() << std::endl;
        inert_odo.imu_compensator.UpdateAccelBiases(avgbiases);
        estimating_biases = 1;
      }

    }

    inert_odo.setPositionState(INS_POS_WINDUP_BALANCE*InerO.P + (1.-INS_POS_WINDUP_BALANCE)*LeggO.P);

    if (true) {

      inert_odo.setVelocityState(INS_VEL_WINDUP_BALANCE*InerO.V + (1.-INS_VEL_WINDUP_BALANCE)*LeggO.V);
      } else {
      //inert_odo.setVelocityState(INS_VEL_WINDUP_BALANCE*InerO.V + (1.-INS_VEL_WINDUP_BALANCE)*Fovis.V);
    }

    if (_switches->verbose) {
      std::cout << "LeggO: " << LeggO.P.transpose() << std::endl
            << "InerO: " << InerO.P.transpose() << std::endl
            << "dP_w : " << (LeggO.P - InerO.P).transpose() << std::endl
            << "IO.q : " << InerO.lQb.w() << ", " << InerO.lQb.x() << ", " << InerO.lQb.y() << ", " << InerO.lQb.z() << std::endl
            << "err_b: " << err_b.transpose() << std::endl
            << "a_b_measured: " << just_checking_imu_frame.a_b_measured.transpose() << std::endl
            << "ba_b: " << inert_odo.imu_compensator.getAccelBiases().transpose() << std::endl
            << "a_b: " << just_checking_imu_frame.a_b.transpose() << std::endl
            << "a_l " << just_checking_imu_frame.a_l.transpose() << std::endl
            << "f_l " << just_checking_imu_frame.f_l.transpose() << std::endl
            << "a gain " << err_b.norm() << " | "<< a << ", " << (0.6+0.3*(a)) << ", " << 0.3*(1-a)+0.1 << ", " << (0.6+0.3*(a))+0.3*(1-a)+0.1 << std::endl
            << "C_w2b: " << std::endl << q2C(InerO.lQb).transpose() << std::endl
            << std::endl;
    }

  }

  retstate = InerO;
  //std::cout << "InertOdo.V: " << InerO.V.transpose() << std::endl;
  retstate.P = LeggO.P;

  return retstate;

}

void LegOdometry_Handler::robot_state_handler(  const lcm::ReceiveBuffer* rbuf, 
                        const std::string& channel, 
                        const drc::robot_state_t* _msg) {

  //clock_gettime(CLOCK_REALTIME, &before);
  if (_switches->print_computation_time) {
    gettimeofday(&before,NULL);
  }

  if (_msg->utime - prev_frame_utime > 1000) {
    std::cerr << _msg->utime << " @ " << (_msg->utime - prev_frame_utime)/1000 << " frames were missed\n";
  }

  prev_frame_utime = _msg->utime;

  //std::cout << before.tv_nsec << ", " << spare.tv_nsec << std::endl;
  //spare_time = (before.tv_nsec) - (spare.tv_nsec);


  // The intention is to build up the information inside these messages and pass them out on LCM to who ever needs to consume them
  // The estimated state message variables are created here for two main reasons:
  // 1. Timing of the messaging sending is slaved to the reception of joint angle measurements
  // 2. After an estimation iteration these state variables go out of scope, preventing stale data to be carried over to the next iteration of this code (short of memory errors -- which must NEVER happen)
  // This is the main core memory of the state estimation process. These values are the single point of interface with the LCM cloud -- method of odometry will have their own state memory,
  // and should always be managed as such.
  drc::robot_state_t est_msgout;
  bot_core::pose_t est_headmsg;
  Eigen::Isometry3d left;
  Eigen::Isometry3d right;
  bool legchangeflag;
  
  //int joints_were_updated=0;


  double left_force, right_force;


  ParseFootForces(_msg, left_force, right_force);

  std::cout << " mf, DetermineLegContactStates\n";
  DetermineLegContactStates((long)_msg->utime,left_force,right_force); // should we have a separate foot contact state classifier, which is not embedded in the leg odometry estimation process
  if (_switches->publish_footcontact_states) {
    //std::cout << "Foot forces are: " << left_force << ", " << right_force << std::endl;
    PublishFootContactEst(_msg->utime);
  }
  
#ifdef TRUE_ROBOT_STATE_MSG_AVAILABLE
  // maintain a true pelvis position for drawing of the foot
  true_pelvis.translation() << _msg->pose.translation.x, _msg->pose.translation.y, _msg->pose.translation.z;

  Eigen::Quaterniond tq(_msg->pose.rotation.w, _msg->pose.rotation.x, _msg->pose.rotation.y, _msg->pose.rotation.z);

  //true_pelvis.rotate(tq);
  true_pelvis.linear() = q2C(tq);

  // TODO -- This is to be removed, only using this for testing
  //_leg_odo->setTruthE(InertialOdometry::QuaternionLib::q2e(tq));
  //std::cout << "true check\n";
  _leg_odo->setTruthE(q2e_new(tq));

#endif

  // Here we start populating the estimated robot state data
  // TODO -- Measure the computation time required for this copy operation.
  est_msgout = *_msg;

  FilterHandForces(_msg, &est_msgout);


  int ratechangeiter=0;

  if (_switches->do_estimation){
    
    double alljoints[_msg->num_joints];
    std::string jointnames[_msg->num_joints];
    map<std::string, double> jointpos_in;
    Eigen::Isometry3d current_pelvis;
    Eigen::VectorXd pelvis_velocity(3);

    getJoints(_msg, alljoints, jointnames);
    joints_to_map(alljoints,jointnames, _msg->num_joints, &jointpos_in);
    getTransforms_FK(_msg->utime, jointpos_in, left,right);// FK, translations in body frame with no rotation (I)

    // TODO -- Initialization before the VRC..
    if (firstpass>0)
    {
      firstpass--;// = false;

      if (_switches->grab_true_init) {
        _leg_odo->ResetWithLeftFootStates(left,right,true_pelvis);


      } else {

        Eigen::Isometry3d init_state;
        init_state.setIdentity();

        _leg_odo->ResetWithLeftFootStates(left,right,init_state);

      }
    }

    // This must be broken into separate position and velocity states
    legchangeflag = _leg_odo->UpdateStates(_msg->utime, left, right, left_force, right_force); //footstep propagation happens in here -- we assume that body to world quaternion is magically updated by torso_imu
    if (legchangeflag) {
      persistlegchangeflag = true; // this is to bridge the rate change gap
    }

    current_pelvis = _leg_odo->getPelvisState();


    double pos[3];
    double vel[3];


    if (_switches->OPTION_A) {

      // median filter
      pos[0] = median_filter[0].processSample(current_pelvis.translation().x());
      pos[1] = median_filter[1].processSample(current_pelvis.translation().y());
      pos[2] = median_filter[2].processSample(current_pelvis.translation().z());

      current_pelvis.translation().x() = pos[0];
      current_pelvis.translation().y() = pos[1];
      current_pelvis.translation().z() = pos[2];

      stageA[0] = pos[0];
      stageA[1] = pos[1];
      stageA[2] = pos[2];

      // DD
      _leg_odo->calculateUpdateVelocityStates(_msg->utime, current_pelvis);

      stageB[0] = _leg_odo->getPelvisVelocityStates()(0);
      stageB[1] = _leg_odo->getPelvisVelocityStates()(1);
      stageB[2] = _leg_odo->getPelvisVelocityStates()(2);

      // Rate change
      ratechangeiter = rate_changer.genericRateChange(_msg->utime, _leg_odo->getPelvisVelocityStates(), pelvis_velocity);
      if (ratechangeiter==1) {
        _leg_odo->overwritePelvisVelocity(pelvis_velocity);
      }

      stageC[0] = pelvis_velocity(0);
      stageC[1] = pelvis_velocity(1);
      stageC[2] = pelvis_velocity(2);
    }

    if (_switches->OPTION_B) {

      // Dist Diff
      _leg_odo->calculateUpdateVelocityStates(_msg->utime, current_pelvis);

      stageA[0] = _leg_odo->getPelvisVelocityStates()(0);
      stageA[1] = _leg_odo->getPelvisVelocityStates()(1);
      stageA[2] = _leg_odo->getPelvisVelocityStates()(2);

      // Median
      pelvis_velocity(0) = median_filter[0].processSample(_leg_odo->getPelvisVelocityStates()(0));
      pelvis_velocity(1) = median_filter[1].processSample(_leg_odo->getPelvisVelocityStates()(1));
      pelvis_velocity(2) = median_filter[2].processSample(_leg_odo->getPelvisVelocityStates()(2));

      _leg_odo->overwritePelvisVelocity(pelvis_velocity);

      stageB[0] = pelvis_velocity(0);
      stageB[1] = pelvis_velocity(1);
      stageB[2] = pelvis_velocity(2);


      // Rate change
      ratechangeiter = rate_changer.genericRateChange(_msg->utime, _leg_odo->getPelvisVelocityStates(), pelvis_velocity);
      if (ratechangeiter==1) {
        _leg_odo->overwritePelvisVelocity(pelvis_velocity);
      }
    }

    if (_switches->OPTION_C  || _switches->OPTION_D) {

      Eigen::Vector3d store_vel_state;
      Eigen::VectorXd filtered_pelvis_vel(3);

      Eigen::Vector3d pos;
      pos << current_pelvis.translation().x(),current_pelvis.translation().y(),current_pelvis.translation().z();

      store_vel_state = stageA_test_vel.diff(_msg->utime,pos);

      stageA[0] = store_vel_state(0);
      stageA[1] = store_vel_state(1);
      stageA[2] = store_vel_state(2);

      // DD
      _leg_odo->calculateUpdateVelocityStates(_msg->utime, current_pelvis);

      stageB[0] = _leg_odo->getPelvisVelocityStates()(0);
      stageB[1] = _leg_odo->getPelvisVelocityStates()(1);
      stageB[2] = _leg_odo->getPelvisVelocityStates()(2);

      // Rate change
      store_vel_state = _leg_odo->getPelvisVelocityStates();
      ratechangeiter = rate_changer.genericRateChange(_msg->utime,store_vel_state,filtered_pelvis_vel);

      _leg_odo->overwritePelvisVelocity(filtered_pelvis_vel);

      if (ratechangeiter==1) {
        stageC[0] = filtered_pelvis_vel(0);
        stageC[1] = filtered_pelvis_vel(1);
        stageC[2] = filtered_pelvis_vel(2);

        // Median Filter
        vel[0] = median_filter[0].processSample(filtered_pelvis_vel(0));
        vel[1] = median_filter[1].processSample(filtered_pelvis_vel(1));
        vel[2] = median_filter[2].processSample(filtered_pelvis_vel(2));

        for (int i=0;i<3;i++) {
          filtered_pelvis_vel(i) = vel[i];
        }
        _leg_odo->overwritePelvisVelocity(filtered_pelvis_vel);

      }
    }

    // At this point the pelvis position has been found from leg kinematics


    // Timing profile. This is the midway point
    //clock_gettime(CLOCK_REALTIME, &mid);
    gettimeofday(&mid,NULL);
    // Here the rate change is propagated into the rest of the system

    if (ratechangeiter==1) {

      InertialOdometry::DynamicState datafusion_out;

      // TODO
      LeggO.P << current_pelvis.translation().x(),current_pelvis.translation().y(),current_pelvis.translation().z();
      LeggO.V = _leg_odo->getPelvisVelocityStates();


      datafusion_out = data_fusion(_msg->utime, LeggO, InerOdoEst, FovisEst);


      if (_switches->OPTION_D) {
        // This is for the computations that follow -- not directly leg odometry position
        /*
        current_pelvis.translation().x() = datafusion_out.P(0);
        current_pelvis.translation().y() = datafusion_out.P(1);
        current_pelvis.translation().z() = datafusion_out.P(2);
         */

        _leg_odo->overwritePelvisVelocity(datafusion_out.V);
      }

      if (isnan((float)datafusion_out.V(0)) || isnan((float)datafusion_out.V(1)) || isnan((float)datafusion_out.V(2))) {
        std::cout << "LegOdometry_Handler::robot_state_handler -- NAN happened\n";

        std::cout << "LeggO.P: " << LeggO.P.transpose() << std::endl
              << "LeggO.V: " << LeggO.V.transpose() << std::endl
              << "InerOdoEst.P: " << InerOdoEst.P.transpose() << std::endl
              << "InerOdoEst.V: " << InerOdoEst.V.transpose() << std::endl;

      }


      //clock_gettime(CLOCK_REALTIME, &threequat);
      //std::cout << "Standing on: " << (_leg_odo->foot_contact->getStandingFoot()==LEFTFOOT ? "LEFT" : "RIGHT" ) << std::endl;

      if (imu_msg_received) {
        PublishEstimatedStates(_msg, &est_msgout);
        UpdateHeadStates(&est_msgout, &est_headmsg);
        PublishHeadStateMsgs(&est_headmsg);
        PublishH2B((unsigned long long)_msg->utime, body_to_head.inverse());

        // publishing of the estimated biases
        publishAccBiasEst((unsigned long long)_msg->utime);

      }

      #ifdef TRUE_ROBOT_STATE_MSG_AVAILABLE
      // True state messages will ont be available during the VRC and must be removed accordingly
      //PublishPoseBodyTrue(_msg);
      #endif

      if (_switches->log_data_files) {
        LogAllStateData(_msg, &est_msgout);
      }

    }// end of the reduced rate portion

    }//do estimation

  if (_switches->draw_footsteps) {
    DrawDebugPoses(left, right, _leg_odo->getPelvisState(), legchangeflag);
  }
 
   if (_switches->print_computation_time) {
     //clock_gettime(CLOCK_REALTIME, &after);
       gettimeofday(&after,NULL);
      long long elapsed;
       /*elapsed = static_cast<long>(mid.tv_nsec) - static_cast<long>(before.tv_nsec);
       elapsed_us = elapsed/1000.;
       std::cout << "0.50, " << elapsed_us << ", ";// << std::endl;

       elapsed = static_cast<long>(threequat.tv_nsec) - static_cast<long>(before.tv_nsec);
       elapsed_us = elapsed/1000.;
       std::cout << "0.75, " << elapsed_us << ", ";// << std::endl;
       */

    //elapsed = (long long)(static_cast<long long>(spare.tv_usec) - static_cast<long long>(before.tv_usec));
     elapsed = (after.tv_usec) - (before.tv_usec);

    if (elapsed<=0) {
      std::cout << "Negative elapsed time\n";

    }

    if (elapsed > maxtime) {
      maxtime = elapsed;
    }

    int time_avg_wind = 100;

    elapsed_us += elapsed;
    spare_us += spare_time;

    time_avg_counter++;
    if (time_avg_counter >= time_avg_wind) {
      elapsed_us = elapsed_us/time_avg_wind;
      spare_us = spare_us/time_avg_wind;
      std::cout << "MAX: " << maxtime << " | AVG computation time: [" << elapsed_us << " us]" << std::endl;//, with [" << spare_us << " us] spare" << std::endl;
      spare_us = 0;
      elapsed_us = 0.;
      time_avg_counter = 0;
    }

    //clock_gettime(CLOCK_REALTIME, &spare);
    gettimeofday(&spare,NULL);
   }


}

void LegOdometry_Handler::publishAccBiasEst(const unsigned long long &uts) {
  Eigen::VectorXd dummy(1);

  //std::cout << "wanting to publish\n";

  if (bias_publishing_rate.genericRateChange(uts,dummy,dummy)) {

    drc::estimated_biases_t msg;

    Eigen::Vector3d biases;
    biases = inert_odo.imu_compensator.getAccelBiases();

    msg.utime = uts;
    msg.x = (float)biases(0);
    msg.y = (float)biases(1);
    msg.z = (float)biases(2);

    msg.mode = (char)estimating_biases;
    if (estimating_biases == 0 || !biasmessagesent) {
      lcm_->publish("ESTIMATED_ACCEL_BIASES" + _channel_extension, &msg);
    }
    if (estimating_biases == 1) {
      biasmessagesent = true;
    }
  }
}

void LegOdometry_Handler::DrawDebugPoses(const Eigen::Isometry3d &left, const Eigen::Isometry3d &right, const Eigen::Isometry3d &true_pelvis, const bool &legchangeflag) {

#ifdef DRAW_DEBUG_LEGTRANSFORM_POSES
  // This adds a large amount of computation by not clearing the list -- not optimal, but not worth fixing at the moment

  DrawLegPoses(left, right, true_pelvis);
  // This sendCollection call will be overwritten by the one below -- moved here after testing of the forward kinematics
  _viewer->sendCollection(*_obj_leg_poses, true);
#endif

  if (legchangeflag)
  {
    //std::cout << "LEGCHANGE\n";
    addIsometryPose(collectionindex,_leg_odo->getPrimaryInLocal());
    collectionindex++;
    addIsometryPose(collectionindex,_leg_odo->getPrimaryInLocal());
    collectionindex++;
    _viewer->sendCollection(*_obj, true);
  }

  _viewer->sendCollection(*_obj_leg_poses, true);

}

void LegOdometry_Handler::PublishH2B(const unsigned long long &utime, const Eigen::Isometry3d &h2b) {

  Eigen::Quaterniond h2bq = C2q(h2b.linear());

  bot_core::rigid_transform_t tf;
  tf.utime = utime;
  tf.trans[0] = h2b.translation().x();
  tf.trans[1] = h2b.translation().y();
  tf.trans[2] = h2b.translation().z();

  tf.quat[0] = h2bq.w();
  tf.quat[1] = h2bq.x();
  tf.quat[2] = h2bq.y();
  tf.quat[3] = h2bq.z();

  lcm_->publish("HEAD_TO_BODY" + _channel_extension, &tf);
}

void LegOdometry_Handler::PublishEstimatedStates(const drc::robot_state_t * msg, drc::robot_state_t * est_msgout) {
  
  /*
    if (((!pose_initialized_) || (!vo_initialized_))  || (!zheight_initialized_)) {
      std::cout << "pose or vo or zheight not initialized, refusing to publish EST_ROBOT_STATE\n";
      return;
    }
    */

  drc::position_3d_t origin;
  drc::twist_t twist;
    Eigen::Quaterniond true_q;
    Eigen::Vector3d E_true;
    Eigen::Vector3d E_est;
    bot_core::pose_t pose;

  
  Eigen::Isometry3d currentPelvis   = _leg_odo->getPelvisState();
  Eigen::Vector3d   velocity_states = _leg_odo->getPelvisVelocityStates();
  Eigen::Vector3d   local_rates     = _leg_odo->getLocalFrameRates();
  
  // estimated orientation 
    Eigen::Quaterniond output_q(currentPelvis.linear()); // This is worth checking again
    
    //std::cout << "CHECKING ROTATIONS: " << 57.29*(_leg_odo->truth_E-InertialOdometry::QuaternionLib::q2e(output_q)).norm() << std::endl;

    true_q.w() = msg->pose.rotation.w;
    true_q.x() = msg->pose.rotation.x;
    true_q.y() = msg->pose.rotation.y;
    true_q.z() = msg->pose.rotation.z;

    pose.utime  =msg->utime;
    
    for (int i=0; i<3; i++) {
      pose.vel[i] =velocity_states(i);
      pose.rotation_rate[i] = local_rates(i);
    }
  
    /*
  // True or estimated position
  if (_switches->use_true_z) {

  pose.pos[0] = msg->pose.translation.x;
  pose.pos[1] = msg->pose.translation.y;
  pose.pos[2] = msg->pose.translation.z;

  pose.orientation[0] =true_q.w();
  pose.orientation[1] =true_q.x();
  pose.orientation[2] =true_q.y();
  pose.orientation[3] =true_q.z();

  origin.translation.x = msg->pose.translation.x;
  origin.translation.y = msg->pose.translation.y;
  origin.translation.z = msg->pose.translation.z;

  origin.rotation.w = msg->pose.rotation.w;
  origin.rotation.x = msg->pose.rotation.x;
  origin.rotation.y = msg->pose.rotation.y;
  origin.rotation.z = msg->pose.rotation.z;

  twist.linear_velocity.x = msg->twist.linear_velocity.x; //local_to_body_lin_rate_(0);
  twist.linear_velocity.y = msg->twist.linear_velocity.y; //local_to_body_lin_rate_(1);
  twist.linear_velocity.z = msg->twist.linear_velocity.z; //local_to_body_lin_rate_(2);

  twist.angular_velocity.x = msg->twist.angular_velocity.x;
  twist.angular_velocity.y = msg->twist.angular_velocity.y;
  twist.angular_velocity.z = msg->twist.angular_velocity.z;
  } else {*/
  pose.pos[0] =currentPelvis.translation().x();
  pose.pos[1] =currentPelvis.translation().y();
  pose.pos[2] =currentPelvis.translation().z();

  pose.orientation[0] =output_q.w();
  pose.orientation[1] =output_q.x();
  pose.orientation[2] =output_q.y();
  pose.orientation[3] =output_q.z();

  origin.translation.x = currentPelvis.translation().x();
  origin.translation.y = currentPelvis.translation().y();
  origin.translation.z = currentPelvis.translation().z();

  origin.rotation.w = output_q.w();
  origin.rotation.x = output_q.x();
  origin.rotation.y = output_q.y();
  origin.rotation.z = output_q.z();

  twist.linear_velocity.x = velocity_states(0);//msg->twist.linear_velocity.x;//velocity_states(0);
  twist.linear_velocity.y = velocity_states(1);//msg->twist.linear_velocity.y;//velocity_states(1);
  twist.linear_velocity.z = velocity_states(2);
  publishvelocitiesvec(0) = twist.linear_velocity.x;
  publishvelocitiesvec(1) = twist.linear_velocity.y;
  publishvelocitiesvec(2) = twist.linear_velocity.z;


  //Eigen::Vector3d wrates;
  //wrates = InertialOdometry::QuaternionLib::q2C(output_q).transpose()*local_rates;

  twist.angular_velocity.x = local_rates(0);
  twist.angular_velocity.y = local_rates(1);
  twist.angular_velocity.z = local_rates(2);
  //}

  // EST is TRUE with sensor estimated position
  
  //msgout = *msg;
  est_msgout->pose = origin;
  est_msgout->twist = twist;

  lcm_->publish("EST_ROBOT_STATE" + _channel_extension, est_msgout);


  // TODO -- This must be converted to a permanent foot triad
  // this is to draw the foot position
  /*
  pose.pos[0] = footslidetriad.translation().x();
  pose.pos[1] = footslidetriad.translation().y();
  pose.pos[2] = footslidetriad.translation().z();

  Eigen::Quaterniond footslideq;
  footslideq = C2q(footslidetriad.linear());

  pose.orientation[0] =footslideq.w();
  pose.orientation[1] =footslideq.x();
  pose.orientation[2] =footslideq.y();
  pose.orientation[3] =footslideq.z();
  */

  lcm_->publish("POSE_BODY",&pose);

  /*
  bot_core::pose_t static_foot;

  static_foot.pos[0] = _leg_odo->getPrimaryInLocal().translation().x();
  static_foot.pos[1] = _leg_odo->getPrimaryInLocal().translation().y();
  static_foot.pos[2] = _leg_odo->getPrimaryInLocal().translation().z();

  Eigen::Quaterniond staticfootq;
  staticfootq = C2q(_leg_odo->getPrimaryInLocal().linear());

  //std::cout << "static_foot: " << staticfootq.w() << ", " << staticfootq.x() << ", " << staticfootq.y() << ", " << staticfootq.z() << std::endl;

  static_foot.orientation[0] =staticfootq.w();
  static_foot.orientation[1] =staticfootq.x();
  static_foot.orientation[2] =staticfootq.y();
  static_foot.orientation[3] =staticfootq.z();

  lcm_->publish("STATIC_FOOT",&static_foot);

    bot_core::pose_t compensated_foot;

    compensated_foot.pos[0] = compensated_foot_states.translation().x();
    compensated_foot.pos[1] = compensated_foot_states.translation().y();
    compensated_foot.pos[2] = compensated_foot_states.translation().z();

  Eigen::Quaterniond compensatedfootq;
  compensatedfootq = C2q(compensated_foot_states.linear());

  //std::cout << "static_foot: " << staticfootq.w() << ", " << staticfootq.x() << ", " << staticfootq.y() << ", " << staticfootq.z() << std::endl;

  compensated_foot.orientation[0] =compensatedfootq.w();
  compensated_foot.orientation[1] =compensatedfootq.x();
  compensated_foot.orientation[2] =compensatedfootq.y();
  compensated_foot.orientation[3] =compensatedfootq.z();

  lcm_->publish("COMPENSATED_FOOT",&compensated_foot);
  */


  /*
  // TODO -- remove this pulse train, only for testing
  // now add a 40Hz pulse train to the true robot state
  if (msg->utime*1.E-3 - pulse_time_ >= 2000) {
    pulse_time_ = msg->utime*1.E-3;
    pulse_counter = 16;
  }
  if (pulse_counter>0) {
    origin.translation.x = origin.translation.x + 0.003;
    origin.translation.y = origin.translation.y + 0.003;

    twist.linear_velocity.x = twist.linear_velocity.x + 0.03;
    twist.linear_velocity.y = twist.linear_velocity.y + 0.1;
    pulse_counter--;
  }*/
}

// TODO -- remember that this is to be depreciated
// This function will not be available during the VRC, as the true robot state will not be known
void LegOdometry_Handler::PublishPoseBodyTrue(const drc::robot_state_t * msg) {

  // Infer the Robot's head position from the ground truth root world pose
  bot_core::pose_t pose_msg;
  pose_msg.utime = msg->utime;
  pose_msg.pos[0] = msg->pose.translation.x;
  pose_msg.pos[1] = msg->pose.translation.y;
  pose_msg.pos[2] = msg->pose.translation.z;
  pose_msg.orientation[0] = msg->pose.rotation.w;
  pose_msg.orientation[1] = msg->pose.rotation.x;
  pose_msg.orientation[2] = msg->pose.rotation.y;
  pose_msg.orientation[3] = msg->pose.rotation.z;

  lcm_->publish("POSE_BODY_TRUE", &pose_msg);
}

void LegOdometry_Handler::PublishHeadStateMsgs(const bot_core::pose_t * msg) {

  lcm_->publish("POSE_HEAD" + _channel_extension, msg);

  return;
}

void LegOdometry_Handler::UpdateHeadStates(const drc::robot_state_t * msg, bot_core::pose_t * l2head_msg) {

  Eigen::Vector3d local_to_head_vel;
  Eigen::Vector3d local_to_head_acc;
  Eigen::Vector3d local_to_head_rate;

  Eigen::Isometry3d local_to_head;

  // Replace this with FK
  if (false) {
    int status;
    double matx[16];
    status = bot_frames_get_trans_mat_4x4_with_utime( _botframes, "head", "body", msg->utime, matx);
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        body_to_head(i,j) = matx[i*4+j];
      }
    }
  }

  

  //std::cout << body_to_head.translation().transpose() << " is b2h\n";
  Eigen::Isometry3d pelvis;
  Eigen::Quaterniond q(msg->pose.rotation.w, msg->pose.rotation.x, msg->pose.rotation.y, msg->pose.rotation.z);
  // TODO -- remember this flag

  pelvis.setIdentity();
  pelvis.translation() << msg->pose.translation.x, msg->pose.translation.y, msg->pose.translation.z;
  pelvis.linear() = q2C(q);

    //std::cout << truebody.translation().transpose() << " is tb\n";

  local_to_head = pelvis * body_to_head;
    //std::cout << local_to_head.translation().transpose() << " is l2h\n\n";

  local_to_head_vel = local_to_head_vel_diff.diff(msg->utime, local_to_head.translation());
  local_to_head_acc = local_to_head_acc_diff.diff(msg->utime, local_to_head_vel);
  local_to_head_rate = local_to_head_rate_diff.diff(msg->utime, C2e(local_to_head.linear()));

  // estimate the rotational velocity of the head
  Eigen::Quaterniond l2head_rot;
  l2head_rot = C2q(local_to_head.linear());

  l2head_msg->utime = msg->utime;

  l2head_msg->pos[0] = local_to_head.translation().x();
  l2head_msg->pos[1] = local_to_head.translation().y();
  l2head_msg->pos[2] = local_to_head.translation().z();

  l2head_msg->orientation[0] = l2head_rot.w();
  l2head_msg->orientation[1] = l2head_rot.x();
  l2head_msg->orientation[2] = l2head_rot.y();
  l2head_msg->orientation[3] = l2head_rot.z();

  //head_vel_filters
  for (int i=0;i<3;i++) {
    l2head_msg->vel[i]=head_vel_filters[i].processSample(local_to_head_vel(i));
  }
  //l2head_msg->vel[1]=local_to_head_vel(1);
  //l2head_msg->vel[2]=local_to_head_vel(2);

  l2head_msg->rotation_rate[0]=local_to_head_rate(0);//is this the correct ordering of the roll pitch yaw
  l2head_msg->rotation_rate[1]=local_to_head_rate(1);// Maurice has it the other way round.. ypr
  l2head_msg->rotation_rate[2]=local_to_head_rate(2);
  l2head_msg->accel[0]=local_to_head_acc(0);
  l2head_msg->accel[1]=local_to_head_acc(1);
  l2head_msg->accel[2]=local_to_head_acc(2);

  return;
}

void LegOdometry_Handler::LogAllStateData(const drc::robot_state_t * msg, const drc::robot_state_t * est_msgout) {

  // Logging csv file with true and estimated states
  stringstream ss (stringstream::in | stringstream::out);

  // The true states are
  stateMessage_to_stream(msg, ss);
  stateMessage_to_stream(est_msgout, ss);

  {
  Eigen::Vector3d elogged;
  elogged = q2e_new(Eigen::Quaterniond(est_msgout->pose.rotation.w, est_msgout->pose.rotation.x, est_msgout->pose.rotation.y, est_msgout->pose.rotation.z));

  //std::cout << "logged: " << (_leg_odo->truth_E - elogged).norm() << std::endl;

  }

  // adding timestamp a bit late, sorry
  ss << msg->utime << ", ";

  // Adding the foot forces
  ss << msg->force_torque.l_foot_force_z << ", "; // left
  ss << msg->force_torque.r_foot_force_z << ", "; // right

  // Active foot is
  ss << (_leg_odo->foot_contact->getStandingFoot() == LEFTFOOT ? "0" : "1") << ", ";

  // The single foot contact states are also written to file for reference -- even though its published by a separate processing using this same class.
  ss << _leg_odo->foot_contact->leftContactStatus() << ", ";
  ss << _leg_odo->foot_contact->rightContactStatus() << ", "; // 30

  for (int i=0;i<16;i++) {
    ss << joint_positions[i] << ", "; //59-74
  }

  #ifdef LOG_LEG_TRANSFORMS
     // left vel, right vel, left rate, right rate
     for (int i=0;i<12;i++) {
       ss << pelvis_to_feet_transform[i] << ", ";//91-102
     }
  #endif

   for (int i=0;i<16;i++) {
   ss << filtered_joints[i] << ", "; //103-118
   }

   //std::cout << "Stage A: ";
   for (int i=0;i<3;i++) {
   ss << stageA[i] << ", "; //119-121
   //std::cout << stageA[i] << ", ";
   }
   //std::cout << std::endl;
   for (int i=0;i<3;i++) {
   ss << stageB[i] << ", "; //122-124
   }
   for (int i=0;i<3;i++) {
   ss << stageC[i] << ", "; //125-127
   }

   for (int i=0;i<3;i++) {
     ss << InerOdoEst.P(i) << ", "; //128-130
   }
   for (int i=0;i<3;i++) {
     ss << InerOdoEst.V(i) << ", ";//131-133
   }

   for (int i=0;i<3;i++) {
        ss << LeggO.V(i) << ", ";//134-136
   }

   for (int i=0;i<3;i++) {
     ss << FovisEst.V(i) << ", ";//137-139
   }

   Eigen::Vector3d biasesa;

   biasesa = inert_odo.imu_compensator.getAccelBiases();

   for (int i=0;i<3;i++) {
        ss << biasesa(i) << ", ";//140-142
   }

   for (int i=0;i<6;i++) {
       ss << feedback_loggings[i] << ", "; //143-148
   }

   for (int i=0;i<3;i++) {
    ss << just_checking_imu_frame.f_l(i) << ", "; //149-151
   }

   ss << zvu_flag << ", "; //152

   for (int i=0;i<3;i++) {
     ss << publishvelocitiesvec(i) << ", ";//153-155
   }


   ss <<std::endl;

   state_estimate_error_log << ss.str();

}

// Push the state values in a drc::robot_state_t message type to the given stringstream
void LegOdometry_Handler::stateMessage_to_stream(  const drc::robot_state_t *msg,
                          stringstream &ss) {

  Eigen::Quaterniond q(msg->pose.rotation.w, msg->pose.rotation.x, msg->pose.rotation.y, msg->pose.rotation.z);
  Eigen::Vector3d E;

  E = q2e_new(q);

  ss << msg->pose.translation.x << ", ";
  ss << msg->pose.translation.y << ", ";
  ss << msg->pose.translation.z << ", ";

  ss << msg->twist.linear_velocity.x << ", ";
  ss << msg->twist.linear_velocity.y << ", ";
  ss << msg->twist.linear_velocity.z << ", ";

  ss << E(0) << ", ";
  ss << E(1) << ", ";
  ss << E(2) << ", ";

  ss << msg->twist.angular_velocity.x << ", ";
  ss << msg->twist.angular_velocity.y << ", ";
  ss << msg->twist.angular_velocity.z << ", ";

  return;
}

void LegOdometry_Handler::torso_imu_handler(  const lcm::ReceiveBuffer* rbuf, 
                        const std::string& channel, 
                        const  drc::imu_t* msg) {
  


  double rates[3];
  double accels[3];
  double angles[3];



  if (isnan((float)msg->angular_velocity[0]) || isnan((float)msg->angular_velocity[1]) || isnan((float)msg->angular_velocity[2]) || isnan((float)msg->linear_acceleration[0]) || isnan((float)msg->linear_acceleration[1]) || isnan((float)msg->linear_acceleration[2])) {
    std::cerr << "torso_imu_handler -- NAN encountered, skipping this frame\n";
    return;
  }

  Eigen::Quaterniond q(msg->orientation[0],msg->orientation[1],msg->orientation[2],msg->orientation[3]);

  if (q.norm() <= 0.95) {
    std::cerr << "LegOdometry_Handler::torso_imu_handler -- Non unit quaternion encountered, skipping this frame.\n";
    return;
  }
  
  // To filter or not to filter the angular rates
  if (false) {
    Eigen::Vector3d E;
    E = q2e_new(q);// change to new

    for (int i=0;i<3;i++) {
      rates[i] = lpfilter[i+2].processSample(msg->angular_velocity[i]); // +2 since the foot force values use the first two filters
      angles[i] = lpfilter[i+5].processSample(E(i));
      q = e2q(E);
    }
  } else {
    for (int i=0;i<3;i++) {
      rates[i] = msg->angular_velocity[i]; // +2 since the foot force values use the first two filters
      accels[i] = msg->linear_acceleration[i];

      if (isnan((float)accels[i])) {
        std::cout << "LegOdometry_Handler::torso_imu_handler -- NAN in accel, so ignoring this frame.\n";
      }
    }
  }
  


  Eigen::Vector3d rates_b(rates[0], rates[1], rates[2]);

  _leg_odo->setOrientationTransform(q, rates_b);

  InertialOdometry::IMU_dataframe imu_data;

  imu_data.uts = msg->utime;


  imu_data.a_b_measured = Eigen::Vector3d(accels[0],accels[1],accels[2]);


  //Eigen::Quaterniond trivial_OL_q;

  //trivial_OL_q.setIdentity();

  //trivial_OL_q = e2q(Eigen::Vector3d(PI/2*0.,-PI/4., 0.*PI/2));

  //imu_data.acc_b << 0.1+9.81/sqrt(2), 0., +9.81/sqrt(2);

  just_checking_imu_frame = imu_data;
  InerOdoEst = inert_odo.PropagatePrediction(imu_data,q);

  //std::cout << "T_OL, ut: " << imu_data.uts << ", " << InerOdoEst.V.transpose() << " | " << InerOdoEst.P.transpose() << std::endl;

  if (!imu_msg_received) {
    imu_msg_received = true;
  }

  return;
}


/*
void LegOdometry_Handler::pose_head_true_handler(  const lcm::ReceiveBuffer* rbuf, 
                          const std::string& channel, 
                          const bot_core_pose_t* msg) {

  
}
*/

void LegOdometry_Handler::delta_vo_handler(  const lcm::ReceiveBuffer* rbuf,
                        const std::string& channel,
                        const fovis::update_t* _msg) {


  //std::cout << "LegOdometry_Handler::delta_vo_handler is happening\n";
  Eigen::Isometry3d motion_estimate;
  Eigen::Vector3d vo_dtrans;

  // This is still in the left camera frame and must be rotated to the pelvis frame
  // we are gokng to do this with the forward kinematics
  // we need camera to head -- this is going to go into a struct
  // (all bot transforms calculated here should be moved here for better abstraction in the future)

  vo_dtrans = fk_data.bottransforms.lcam2pelvis( Eigen::Vector3d(_msg->translation[0],_msg->translation[1],_msg->translation[2]));

  // here we need calculate velocity perceived by fovis
  // using the time stamps of the messages and just doing the first order difference on the values


  // need the delta translation in the world frame
  FovisEst.V = (inert_odo.ResolveBodyToRef(vo_dtrans))/((_msg->timestamp - FovisEst.uts)*1E-6); // convert to a world frame velocity

  FovisEst.uts = _msg->timestamp;

}

void LegOdometry_Handler::PublishFootContactEst(int64_t utime) {
  drc::foot_contact_estimate_t msg_contact_est;
  
  msg_contact_est.utime = utime;
  
  // TODO -- Convert this to use the enumerated types from inside the LCM message
  msg_contact_est.detection_method = DIFF_SCHMITT_WITH_DELAY;
  
  msg_contact_est.left_contact = _leg_odo->foot_contact->leftContactStatus();
  msg_contact_est.right_contact = _leg_odo->foot_contact->rightContactStatus();
  
  lcm_->publish("FOOT_CONTACT_ESTIMATE",&msg_contact_est);
}

void LegOdometry_Handler::drawLeftFootPose() {
  //LinkCollection link(2, std::string("Links"));
  
  //addIsometryPose(_leg_odo->getLeftInLocal());
  
  //addIsometryPose(98, _leg_odo->left_to_pelvis);
  
  //TODO - male left_to_pelvis and other private members in TwoLegOdometry class with get functions of the same name, as is done with Eigen::Isometry3d .translation() and .rotation()
  
  addIsometryPose(97, _leg_odo->left_to_pelvis);
  addIsometryPose(98, _leg_odo->left_to_pelvis);
  
  addIsometryPose(87, _leg_odo->pelvis_to_left);
  addIsometryPose(88, _leg_odo->pelvis_to_left);
  
  //InertialOdometry::QuaternionLib::printEulerAngles("drawLeftFootPose()", _leg_odo->pelvis_to_left);
      
}

void LegOdometry_Handler::drawRightFootPose() {
  //LinkCollection link(2, std::string("Links"));
  
  //addIsometryPose(_leg_odo->getLeftInLocal());
  
  
  addIsometryPose(99,_leg_odo->right_to_pelvis);
  addIsometryPose(100,_leg_odo->right_to_pelvis);
  addIsometryPose(89,_leg_odo->pelvis_to_right);
  addIsometryPose(90,_leg_odo->pelvis_to_right);
  
  //std::cout << "adding right foot pose" << std::endl;
}

void LegOdometry_Handler::drawSumPose() {
  //addIsometryPose(95,_leg_odo->add(_leg_odo->left_to_pelvis,_leg_odo->pelvis_to_right));
  //addIsometryPose(96,_leg_odo->add(_leg_odo->left_to_pelvis,_leg_odo->pelvis_to_right));
  
  addIsometryPose(93,_leg_odo->getSecondaryInLocal());
  addIsometryPose(94,_leg_odo->getSecondaryInLocal());
}


void LegOdometry_Handler::addIsometryPose(int objnumber, const Eigen::Isometry3d &target) {
  
  Eigen::Vector3d E;
  E = q2e_new(Eigen::Quaterniond(target.linear()));//change to new
  _obj->add(objnumber, isam::Pose3d(target.translation().x(),target.translation().y(),target.translation().z(),E(2),E(1),E(0)));
}

// Four Isometries must be passed -- representing pelvisto foot and and foot to pelvis transforms
void LegOdometry_Handler::DrawLegPoses(const Eigen::Isometry3d &left, const Eigen::Isometry3d &right, const Eigen::Isometry3d &true_pelvis) {
  
  Eigen::Isometry3d target[4];

  target[0] = left;
  target[1] = right;
  target[2] = left.inverse();
  target[3] = right.inverse();

  Eigen::Vector3d E;
  Eigen::Isometry3d added_vals[2];
  
  Eigen::Isometry3d back_from_feet[2];
  
  //clear the list to prevent memory growth
  _obj_leg_poses->clear();
  

  // The best way to add two isometries -- maintained to be consistent with the rest of the code. This should probably be changed
  for (int i=0;i<2;i++) {
    added_vals[i] = _leg_odo->add(true_pelvis, target[i]); // this is the same function that is used by TwoLegOdometry to accumulate Isometry transforms
    E = q2e_new(Eigen::Quaterniond(added_vals[i].linear()));
    _obj_leg_poses->add(50+i, isam::Pose3d(added_vals[i].translation().x(),added_vals[i].translation().y(),added_vals[i].translation().z(),E(2),E(1),E(0)));
    
    back_from_feet[i] = _leg_odo->add(added_vals[i], target[i+2]);
    E = q2e_new(Eigen::Quaterniond(back_from_feet[i].linear()));
    _obj_leg_poses->add(50+i+2, isam::Pose3d(back_from_feet[i].translation().x(),back_from_feet[i].translation().y(),back_from_feet[i].translation().z(),E(2),E(1),E(0)));
  }
  
}

// this function may be depreciated soon
void LegOdometry_Handler::addFootstepPose_draw() {
  std::cout << "Drawing pose for foot: " << (_leg_odo->foot_contact->getStandingFoot() == LEFTFOOT ? "LEFT" : "RIGHT") << std::endl; 
  _obj->add(collectionindex, isam::Pose3d(_leg_odo->getPrimaryInLocal().translation().x(),_leg_odo->getPrimaryInLocal().translation().y(),_leg_odo->getPrimaryInLocal().translation().z(),0,0,0));  
  collectionindex = collectionindex + 1;
}

void LegOdometry_Handler::getJoints(const drc::robot_state_t * msg, double alljoints[], std::string joint_name[]) {
  
  if (filtered_joints.capacity() != msg->num_joints || !filter_joints_vector_size_set) {
    std::cout << "LegOdometry_Handler::getJoints -- Automatically changing the capacity of filt.joints -- capacity: " << filtered_joints.capacity() << ", num_joints: " << msg->num_joints << std::endl;
    filter_joints_vector_size_set = true;
    filtered_joints.resize(msg->num_joints);
  }



  // call a routine that calculates the transforms the joint_state_t* msg.

  if (first_get_transforms) {
    first_get_transforms = false;
    InitializeFilters((int)msg->num_joints);
  }

  for (uint i=0; i< (uint) msg->num_joints; i++) {
    // Keep joint positions in local memory
    alljoints[i] = msg->joint_position[i];
    joint_name[i] = msg->joint_name[i];

    if (i<16) {
      joint_positions[i] = msg->joint_position[i];
    }

  }

  //filterJointPositions(msg->utime, msg->num_joints, alljoints);
}

void LegOdometry_Handler::joints_to_map(const double joints[], const std::string joint_name[], const int &num_joints, std::map<string, double> *_jointpos_in) {

  for (uint i=0; i< num_joints; i++) { //cast to uint to suppress compiler warning

    switch (2) {
      case 1:
      _jointpos_in->insert(make_pair(joint_name[i], joint_lpfilters.at(i).processSample(joints[i])));
      break;
      case 2:
      // not using filters on the joint position measurements
      _jointpos_in->insert(make_pair(joint_name[i], joints[i]));//skipping the filters
      break;
      //case 3:
      //_jointpos_in->insert(make_pair(msg->joint_name[i], filtered_joints[i])); // The rate has been reduced to sample periods greater than 4500us and filtered with integral/rate/diff
      //break;
    }
  }
}


// TODO -- make a transform to Eigen converter, since this function now has 4 similar conversions
void LegOdometry_Handler::getTransforms_FK(const unsigned long long &u_ts, const map<string, double> &jointpos_in, Eigen::Isometry3d &left, Eigen::Isometry3d &right) {
	
	fk_data.utime = u_ts;
	fk_data.jointpos_in = jointpos_in;
	
	// This will update the left and right leg transforms
	getFKTransforms(fk_data, left, right, body_to_head);

}

// This member function is no longer in use
int LegOdometry_Handler::filterJointPositions(const unsigned long long &ts, const int &num_joints, double alljoints[]) {


  int returnval=0;
  /*Eigen::VectorXd int_vals(num_joints);
  Eigen::VectorXd diff_vals(num_joints);

  //std::cout << " | " << std::fixed << alljoints[5];
  // Integrate to lose noise, but keep information
  int_vals = joint_integrator.integrate(ts, num_joints, alljoints);
  //std::cout << " i: " << int_vals(5);


  // we are looking for a 200Hz process -- 5ms
  if (rate_changer.checkNewRateTrigger(ts)) {
    diff_vals = joint_pos_filter.diff(ts, int_vals);
    //joint_integrator.reset_in_time();

    for (int i=0;i<num_joints;i++) {
      filtered_joints[i] = diff_vals(i);
    }

    std::cout << ", after: " << filtered_joints[5] << "\n";
    returnval = 1;
  }
  */
  return returnval;
}

void LegOdometry_Handler::terminate() {
  std::cout << "Closing and cleaning out LegOdometry_Handler object\n";
  
  _leg_odo->terminate();
}

Eigen::Vector3d LegOdometry_Handler::getInerAccBiases() {

  return inert_odo.imu_compensator.getAccelBiases();
}
