function [imuMsgBatch, sentMsg] = sendDrcAtlasRawIMU(dt, n, data,imuMsgBatch, lc)

%% First create a new imu message to be added to the batch queue

% struct atlas_raw_imu_t
% {
%   int64_t utime;
%   int64_t packet_count;
%   double delta_rotation[3];
%   double linear_acceleration[3];
% }


% We need to introduce the same misalignment of the KVH IMU here, is it is
% mounted on the Atlas robot.
iCb = eye(3);        % We use the trivial case for now
Trans = zeros(3,1);
deltaAng = iCb * data.imu.gyr*dt + Trans;
linAcc = iCb * data.imu.acc + Trans;

imumsg = drc.atlas_raw_imu_t();

imumsg.utime = data.imu.utime;
imumsg.packet_count = n;
imumsg.delta_rotation = deltaAng;
imumsg.linear_acceleration = linAcc;

% Now stack the data together in the batch message, before passing it to
% the state-estimator process

imuMsgBatch = [imumsg, imuMsgBatch(1:14)];


%% Prepare and send the actual message -- this only runs at third rate

% struct atlas_raw_imu_batch_t
% {
%   int64_t utime; // timestamp of corresponding status and state message
%   int32_t num_packets;
%   atlas_raw_imu_t raw_imu[num_packets];
% }

sentMsg = 0;
% rate change to 333Hz, TBC
if (mod(imumsg.utime,3000)==0)

    msg = drc.atlas_raw_imu_batch_t();
    msg.utime = data.imu.utime;
    msg.num_packets = 15;
    msg.raw_imu = imuMsgBatch;

     lc.publish('ATLAS_IMU_BATCH_MS', msg);
     sentMsg = 1;
end
