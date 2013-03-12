#ifndef _maps_PointCloudView_hpp_
#define _maps_PointCloudView_hpp_

#include "ViewBase.hpp"

namespace maps {

class PointCloudView : public ViewBase {
public:
  typedef boost::shared_ptr<PointCloudView> Ptr;
public:
  PointCloudView();
  ~PointCloudView();

  void setResolution(const float iResolution);
  maps::PointCloud::Ptr getPointCloud() const;  

  const Type getType() const;
  ViewBase::Ptr clone() const;
  void set(const maps::PointCloud::Ptr& iCloud);
  maps::PointCloud::Ptr getAsPointCloud(const bool iTransform=true) const;
  maps::TriangleMesh::Ptr getAsMesh(const bool iTransform=true) const;
  bool getClosest(const Eigen::Vector3f& iPoint,
                  Eigen::Vector3f& oPoint, Eigen::Vector3f& oNormal);

protected:
  float mResolution;
  maps::PointCloud::Ptr mCloud;
};

}

#endif
