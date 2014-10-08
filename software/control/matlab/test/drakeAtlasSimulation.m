function drakeAtlasSimulation
visualize = false; % should be a function arg

% load in Atlas initial state
load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));

% silence some warnings
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')

% construct robot model
options.floating = true;
options.ignore_friction = true;
options.dt = 0.005;
options.visualize = false;
options.hokuyo = true;
% TODO: get from LCM
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
r = r.removeCollisionGroupsExcept({'heel','toe'});
r = compile(r);

% set initial state to fixed point
load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));
xstar(1) = 0;
xstar(2) = 0;
xstar(6) = 0;
r = r.setInitialState(xstar);
x0 = xstar;

% LCM interpret in
outs(1).system = 2;
outs(1).output = 1;
outs(2).system = 2;
outs(2).output = 2;
lcmInputBlock = LCMInputFromAtlasCommandBlock(r,options);
sys = mimoFeedback(lcmInputBlock, r, [], [], [], outs);

% LCM broadcast out
lcmBroadcastBlock = LCMBroadcastBlock(r);
sys = mimoCascade(sys, lcmBroadcastBlock);


% Visualize if desired
if visualize
  v = r.constructVisualizer;
  v.display_dt = 0.01;
  S=warning('off','Drake:DrakeSystem:UnsupportedSampleTime');
  output_select(1).system=1;
  output_select(1).output=1;
  sys = mimoCascade(sys,v,[],[],output_select);
  warning(S);
end

% TODO is to move to runLCM so that I don't  have to handle LCM
% parsing within a mimoOutput function. For now that's functioning
% and gives me some useful control over how that's working...
simulate(sys,[0.0,Inf], [], options);