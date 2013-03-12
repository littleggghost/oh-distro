#include "VoxelGridView.hpp"

#include "VoxelGrid.hpp"

using namespace maps;

namespace maps {
class OccupancyGrid : public VoxelGrid<int8_t> {
};
}

VoxelGridView::
VoxelGridView() {
  mGrid.reset(new OccupancyGrid());
  // TODO: set resolution
}

VoxelGridView::
~VoxelGridView() {
}

void VoxelGridView::
setResolution(const float iResX, const float iResY, const float iResZ) {
  // TODO
}

boost::shared_ptr<OccupancyGrid> VoxelGridView::
getGrid() const {
  return mGrid;
}

const ViewBase::Type VoxelGridView::
getType() const {
  return TypeVoxelGrid;
}

ViewBase::Ptr VoxelGridView::
clone() const {
  VoxelGridView* view = new VoxelGridView(*this);
  // TODO: copy grid
  return ViewBase::Ptr(view);
}

void VoxelGridView::
set(const maps::PointCloud::Ptr& iCloud) {
  // TODO
}

maps::PointCloud::Ptr VoxelGridView::
getAsPointCloud(const bool iTransform) const {
  maps::PointCloud::Ptr cloud(new maps::PointCloud());
  // TODO: get cloud and transform
  return cloud;
}

maps::TriangleMesh::Ptr VoxelGridView::
getAsMesh(const bool iTransform) const {
  maps::TriangleMesh::Ptr mesh;
  // TODO
  return mesh;
}

bool VoxelGridView::
getClosest(const Eigen::Vector3f& iPoint,
           Eigen::Vector3f& oPoint, Eigen::Vector3f& oNormal) {
  // TODO: getClosest
  return false;
}
