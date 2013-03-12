#include <chrono>
#include <fstream>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <pcl/range_image/range_image.h>
#include <lcm/lcm-cpp.hpp>
#include <bot_lcmgl_client/lcmgl.h>
#include <maps/SensorDataReceiver.hpp>
#include <maps/MapManager.hpp>
#include <maps/LocalMap.hpp>
#include <maps/Collector.hpp>
#include <maps/Utils.hpp>
#include <maps/PointCloudView.hpp>
#include <maps/RangeImageView.hpp>

using namespace maps;
using namespace std;


class State {
public:
  boost::shared_ptr<lcm::LCM> mLcm;
  boost::shared_ptr<Collector> mCollector;
  int mActiveMapId;
  bot_lcmgl_t* mLcmGl;

  State() {
    mLcm.reset(new lcm::LCM());
    mCollector.reset(new Collector());
    mCollector->setLcm(mLcm);
    mActiveMapId = 0;
    mLcmGl = bot_lcmgl_init(mLcm->getUnderlyingLCM(), "test-points");
  }

  ~State() {
    bot_lcmgl_destroy(mLcmGl);
  }
};

class DataProducer {
protected:
  State* mState;
public:
  DataProducer(State* iState) : mState(iState) {}

  void operator()() {
    while (true) {
      // get submap we created earlier
      LocalMap::Ptr localMap =
        mState->mCollector->getMapManager()->getMap(mState->mActiveMapId);

      // find time range of desired swath (from 45 to 135 degrees)
      int64_t timeMin, timeMax;
      mState->mCollector->getLatestSwath(60*4*atan(1)/180, 120*4*atan(1)/180,
                                       timeMin, timeMax);
      LocalMap::SpaceTimeBounds bounds;
      bounds.mTimeMin = timeMin;
      bounds.mTimeMax = timeMax;

      // get and publish point cloud corresponding to this time
      maps::PointCloud::Ptr cloud =
        localMap->getAsPointCloud(0, bounds)->getPointCloud();
      bot_lcmgl_t* lcmgl = mState->mLcmGl;
      bot_lcmgl_color3f(lcmgl, 0, 1, 0);
      bot_lcmgl_point_size(lcmgl, 3);
      for (int i = 0; i < cloud->size(); ++i) {
        maps::PointCloud::PointType point = (*cloud)[i];
        bot_lcmgl_begin(lcmgl, LCMGL_POINTS);
        bot_lcmgl_vertex3f(lcmgl, point.x, point.y, point.z);
        bot_lcmgl_end(lcmgl);
      }
      bot_lcmgl_switch_buffer(lcmgl);

      // set up sample camera pose
      Eigen::Vector3f trans(0,0,0);
      Eigen::Matrix3f rot;
      rot.col(0) = -Eigen::Vector3f::UnitY();
      rot.col(1) = -Eigen::Vector3f::UnitZ();
      rot.col(2) = Eigen::Vector3f::UnitX();
      Eigen::Isometry3f pose = Eigen::Isometry3f::Identity();
      pose.linear() = rot;
      pose.translation() = trans;

      // set up sample camera projection parameters
      int width(200), height(200);
      Eigen::Matrix3f calib = Eigen::Matrix3f::Identity();
      calib(0,0) = calib(1,1) = 50;  // focal length of 50 pixels
      calib(0,2) = width/2.0;        // cop at center of image
      calib(1,2) = height/2.0;

      // create range image
      Eigen::Projective3f projector;
      Utils::composeViewMatrix(projector, calib, pose, false);
      RangeImageView::Ptr rangeImage =
        localMap->getAsRangeImage(width, height, projector, bounds);

      // get and store range image pixel values
      float* ranges = rangeImage->getRangeImage()->getRangesArray();
      std::ofstream ofs("/home/antone/ranges.txt");
      for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
          ofs << ranges[i*width + j] << " ";
        }
        ofs << std::endl;
      }
      ofs.close();
      std::cout << "Got range image" << std::endl;

      // wait for timer expiry
      boost::asio::io_service service;
      boost::asio::deadline_timer timer(service);
      timer.expires_from_now(boost::posix_time::milliseconds(1*1000));
      timer.wait();
      std::cout << "Timer expired." << std::endl;
    }
  }
};



int main() {
  // set up matrices
  Eigen::Matrix3f calib;
  calib<<1000,3,450,0,2000,500,0,0,9;
  Eigen::Quaternionf q(1,2,3,4);
  q.normalize();
  Eigen::Isometry3f pose = Eigen::Isometry3f::Identity();
  pose.translation() = Eigen::Vector3f(10,20,30);
  pose.linear() = q.matrix();

  // compose a matrix
  Eigen::Projective3f matx;
  bool isOrtho = true;
  std::cout << "orig pose" << std::endl;
  std::cout << pose.matrix() << std::endl;
  std::cout << "orig calib" << std::endl;
  std::cout << calib << std::endl;
  Utils::composeViewMatrix(matx, calib, pose, isOrtho);
  std::cout << "composed matrix" << std::endl;
  std::cout << matx.matrix() << std::endl;

  // factor that matrix
  Utils::factorViewMatrix(matx, calib, pose, isOrtho);
  std::cout << "pose" << std::endl;
  std::cout << pose.matrix() << std::endl;
  std::cout << "calib" << std::endl;
  std::cout << calib << std::endl;
  std::cout << "ortho? " << (isOrtho ? "yes" : "no") << std::endl;

  // compose again; compare
  Eigen::Projective3f matx2;
  Utils::composeViewMatrix(matx2, calib, pose, isOrtho);
  std::cout << "difference" << std::endl;
  std::cout << (matx2.matrix()-matx.matrix()) << std::endl;
  std::cout << "norm " << (matx2.matrix()-matx.matrix()).norm() << std::endl;

  Utils::composeViewMatrix(matx2, calib, Eigen::Isometry3f::Identity(),
                           isOrtho);
  std::cout << "FOO\n" << matx2.matrix() << std::endl;

  return 0;


  // create state object instance
  State state;

  // create new submap
  LocalMap::Spec mapSpec;
  mapSpec.mId = 1;
  mapSpec.mPointBufferSize = 5000;
  mapSpec.mActive = true;
  mapSpec.mBoundMin = Eigen::Vector3f(-1,-1,-1)*10;
  mapSpec.mBoundMax = Eigen::Vector3f(1,1,1)*10;
  mapSpec.mResolution = 0.01;
  state.mActiveMapId = state.mCollector->getMapManager()->createMap(mapSpec);

  // start running wrapper
  std::string laserChannel("SCAN");
  state.mCollector->getDataReceiver()->
    addChannel(laserChannel,
               SensorDataReceiver::SensorTypePlanarLidar,
               laserChannel, "local");
  state.mCollector->start();

  // start producing data
  DataProducer producer(&state);
  boost::thread producerThread(boost::ref(producer));

  // main lcm loop
  while (0 == state.mLcm->handle());

  // join pending threads
  producerThread.join();
}
