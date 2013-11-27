#!/bin/bash

echo "Generate Robot Model Configurations:"
# 9 real robot configurations:
echo "[I]Robot, [N]one, [S]andia, [H]ook, [P]ointer, [R]obotiq"

echo "IRobot left - 3"
rosrun xacro xacro.py xacro/atlas_LI_RI.urdf.xacro > model_LI_RI.urdf
rosrun xacro xacro.py xacro/atlas_LI_RN.urdf.xacro > model_LI_RN.urdf
rosrun xacro xacro.py xacro/atlas_LI_RS.urdf.xacro > model_LI_RS.urdf

echo "None left - 5"
rosrun xacro xacro.py xacro/atlas_LN_RI.urdf.xacro > model_LN_RI.urdf
rosrun xacro xacro.py xacro/atlas_LN_RN.urdf.xacro > model_LN_RN.urdf
rosrun xacro xacro.py xacro/atlas_LN_RS.urdf.xacro > model_LN_RS.urdf
rosrun xacro xacro.py xacro/atlas_LN_RR.urdf.xacro > model_LN_RR.urdf
rosrun xacro xacro.py xacro/atlas_LN_RH.urdf.xacro > model_LN_RH.urdf

echo "Sandia left - 3"
rosrun xacro xacro.py xacro/atlas_LS_RI.urdf.xacro > model_LS_RI.urdf
rosrun xacro xacro.py xacro/atlas_LS_RN.urdf.xacro > model_LS_RN.urdf
rosrun xacro xacro.py xacro/atlas_LS_RS.urdf.xacro > model_LS_RS.urdf

echo "Hook left - 4"
rosrun xacro xacro.py xacro/atlas_LH_RI.urdf.xacro > model_LH_RI.urdf
rosrun xacro xacro.py xacro/atlas_LH_RN.urdf.xacro > model_LH_RN.urdf
rosrun xacro xacro.py xacro/atlas_LH_RH.urdf.xacro > model_LH_RH.urdf
rosrun xacro xacro.py xacro/atlas_LH_RR.urdf.xacro > model_LH_RR.urdf

echo "Pointer left - 0"


echo "Robotiq left - 4"
rosrun xacro xacro.py xacro/atlas_LR_RN.urdf.xacro > model_LR_RN.urdf


# the simulated robot configuration - LCM facing and ROS/gazbo facing versions
echo "17/18 sim - MIT Facing"
rosrun xacro xacro.py xacro/atlas_sandia_hands_sim.urdf.xacro > model_sim.urdf
echo "18/18 sim - Gazebo Facing"
./renameSimGazeboURDF.sh
