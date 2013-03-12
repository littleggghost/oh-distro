#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>

#include <iostream>

#include <gdk/gdkkeysyms.h>
#include <lcm/lcm-cpp.hpp>
#include <bot_vis/bot_vis.h>
#include <bot_param/param_util.h>
#include <bot_frames/bot_frames.h>

#include <lcmtypes/drc/map_command_t.hpp>
#include <lcmtypes/drc/map_macro_t.hpp>

#include <drc_utils/Clock.hpp>

#include <unordered_map>

#include <pcl/common/transforms.h>

#include <maps/ViewClient.hpp>
#include <maps/Utils.hpp>

#include "MeshRenderer.hpp"

static const char* RENDERER_NAME = "Maps";

static const char* PARAM_INPUT_MODE = "Input Mode";
static const char* PARAM_REQUEST_TYPE = "Data Type";
static const char* PARAM_REQUEST_RES = "Resolution (m)";
static const char* PARAM_REQUEST_FREQ = "Frequency (Hz)";
static const char* PARAM_REQUEST_TIME = "Time Window (s)";
static const char* PARAM_REQUEST_RELATIVE = "Relative Position?";
static const char* PARAM_DATA_REQUEST = "Send Request";
static const char* PARAM_MAP_COMMAND_TYPE = "Command";
static const char* PARAM_MAP_COMMAND = "Execute";
static const char* PARAM_MESH_MODE = "Mesh Mode";
static const char* PARAM_COLOR_MODE = "Color Mode";
static const char* PARAM_POINT_SIZE = "Point Size";

enum InputMode {
  INPUT_MODE_CAMERA,
  INPUT_MODE_RECT,
  INPUT_MODE_DEPTH
};

enum DataRequestType {
  REQUEST_TYPE_CLOUD,
  REQUEST_TYPE_OCTREE,
  REQUEST_TYPE_RANGE
};

enum MapCommand {
  MAP_COMMAND_CLEAR,
  MAP_COMMAND_SCAN_HIGH_RES
};

enum ColorMode {
  COLOR_MODE_SOLID,
  COLOR_MODE_HEIGHT,
  COLOR_MODE_RANGE,
  COLOR_MODE_CAMERA
};

enum MeshMode {
  MESH_MODE_POINTS,
  MESH_MODE_SURFELS,
  MESH_MODE_WIREFRAME,
  MESH_MODE_FILLED
};

struct Frustum {
  std::vector<Eigen::Vector4f> mPlanes;
  float mNear;
  float mFar;
  Eigen::Vector3f mPos;
  Eigen::Vector3f mDir;
};

struct RendererMaps;

struct ViewMetaData {
  int64_t mId;
  bool mVisible;
  bool mRelative;
  Frustum mFrustum;
  // TODO maps::MapView::TriangleMesh::Ptr mMesh;
  // maps::MapView::TriangleMesh::Ptr mSurfels;
  Eigen::Vector3f mColor;
};

struct ViewWidget {
  int64_t mId;
  RendererMaps* mRenderer;
  GtkWidget* mBox;
};

struct RendererMaps {

  struct ViewClientListener : public maps::ViewClient::Listener {
    RendererMaps* mRenderer;
    bool mInitialized;

    void notifyData(const int64_t iViewId) {
      mRenderer->addViewMetaData(iViewId);
      ViewMetaData& data = mRenderer->mViewData[iViewId];
      /* TODO
      if ((mRenderer->mMeshMode == MESH_MODE_FILLED) ||
          (mRenderer->mMeshMode == MESH_MODE_WIREFRAME)) {
        data.mMesh = mRenderer->mViewClient.getView(iViewId)->getAsMesh();
      }
      else if (mRenderer->mMeshMode == MESH_MODE_SURFELS) {
        const maps::MapView::SurfelCloud::Ptr surfels =
          mRenderer->mViewClient.getView(iViewId)->getAsSurfels();
        if (data.mSurfels == NULL) {
          data.mSurfels.reset(new maps::MapView::TriangleMesh());
        }
        data.mSurfels->mVertices.reserve(4*surfels->size());
        data.mSurfels->mFaces.reserve(2*surfels->size());
        for (size_t i = 0; i < surfels->size(); ++i) {
          float size = 0.1;  // TODO
          maps::MapView::SurfelCloud::PointType& s = (*surfels)[i];
          Eigen::Vector3f pt(s.x, s.y, s.z);
          Eigen::Vector3f normal(s.normal[0], s.normal[1], s.normal[2]);
          Eigen::Vector3f v2 = Eigen::Vector3f::UnitZ()*size;
          Eigen::Vector3f v1 = v2.cross(normal);
          v1.normalize();
          v1 *= size;
          data.mSurfels->mVertices.push_back(pt-v1+v2);
          data.mSurfels->mVertices.push_back(pt+v1+v2);
          data.mSurfels->mVertices.push_back(pt+v1-v2);
          data.mSurfels->mVertices.push_back(pt-v1-v2);
          data.mSurfels->mFaces.push_back(Eigen::Vector3i(4*i,4*i+3,4*i+1));
          data.mSurfels->mFaces.push_back(Eigen::Vector3i(4*i+1,4*i+3,4*i+2));
        }
      }
      */
      bot_viewer_request_redraw(mRenderer->mViewer);
    }

    void notifyCatalog(const bool iChanged) {
      if (!mInitialized || iChanged) {
        std::vector<maps::ViewClient::ViewPtr> views =
          mRenderer->mViewClient.getAllViews();
        std::set<int64_t> catalogIds;
        for (size_t v = 0; v < views.size(); ++v) {
          int64_t id = views[v]->getId();
          mRenderer->addViewMetaData(id);
          catalogIds.insert(id);
        }
        for (DataMap::iterator iter = mRenderer->mViewData.begin();
             iter != mRenderer->mViewData.end(); ) {
          int64_t id = iter->second.mId;
          if (catalogIds.find(id) == catalogIds.end()) {
            mRenderer->mViewData.erase(iter++);
            mRenderer->removeViewMetaData(id);
          }
          else {
            ++iter;
          }
        }
        mInitialized = true;
        bot_viewer_request_redraw(mRenderer->mViewer);
      }
    }
  };

  BotRenderer mRenderer;
  BotViewer* mViewer;
  boost::shared_ptr<lcm::LCM> mLcm;
  BotParam* mBotParam;
  BotFrames* mBotFrames;
  BotGtkParamWidget* mWidget;
  BotEventHandler mEventHandler;
  GtkWidget* mViewListWidget;

  typedef std::unordered_map<int64_t,ViewWidget> WidgetMap;
  WidgetMap mViewWidgets;

  typedef std::unordered_map<int64_t,ViewMetaData> DataMap;
  DataMap mViewData;
  maps::ViewClient mViewClient;
  ViewClientListener mViewClientListener;

  boost::shared_ptr<maps::MeshRenderer> mMeshRenderer;
  ColorMode mColorMode;
  MeshMode mMeshMode;

  InputMode mInputMode;
  bool mDragging;
  Eigen::Vector2f mDragPoint1;
  Eigen::Vector2f mDragPoint2;
  float mBaseValue;
  int mWhichButton;
  Frustum mFrustum;
  bool mBoxValid;

  DataRequestType mRequestType;
  float mRequestFrequency;
  float mRequestResolution;
  float mRequestTimeWindow;
  bool mRequestRelativeLocation;

  MapCommand mMapCommand;

  RendererMaps() {
    mBoxValid = false;
    mDragging = false;
    mMeshRenderer.reset(new maps::MeshRenderer());
    mMeshRenderer->setPointSize(3);
    mMeshRenderer->setCameraChannel("CAMERALEFT");
    mMeshMode = MESH_MODE_POINTS;
    mColorMode = COLOR_MODE_SOLID;
    mInputMode = INPUT_MODE_CAMERA;
  }

  static void onParamWidgetChanged(BotGtkParamWidget* iWidget,
                                   const char *iName,
                                   RendererMaps *self) {
    self->mInputMode = (InputMode)
      bot_gtk_param_widget_get_enum(iWidget, PARAM_INPUT_MODE);
    self->mRequestType = (DataRequestType)
      bot_gtk_param_widget_get_enum(iWidget, PARAM_REQUEST_TYPE);
    self->mRequestFrequency = (float)
      bot_gtk_param_widget_get_double(iWidget, PARAM_REQUEST_FREQ);
    self->mRequestResolution = (float)
      bot_gtk_param_widget_get_double(iWidget, PARAM_REQUEST_RES);
    self->mRequestTimeWindow = (float)
      bot_gtk_param_widget_get_double(iWidget, PARAM_REQUEST_TIME);
    self->mRequestRelativeLocation = (bool)
      bot_gtk_param_widget_get_bool(iWidget, PARAM_REQUEST_RELATIVE);
    self->mMeshMode = (MeshMode)
      bot_gtk_param_widget_get_enum(iWidget, PARAM_MESH_MODE);
    self->mColorMode = (ColorMode)
      bot_gtk_param_widget_get_enum(iWidget, PARAM_COLOR_MODE);
    float val = (double)
      bot_gtk_param_widget_get_double(iWidget, PARAM_POINT_SIZE);      
    self->mMeshRenderer->setPointSize(val);
      
    self->mMapCommand = (MapCommand)
      bot_gtk_param_widget_get_enum(iWidget, PARAM_MAP_COMMAND_TYPE);

    if (!strcmp(iName, PARAM_MAP_COMMAND)) {
      if (self->mMapCommand == MAP_COMMAND_CLEAR) {
        drc::map_command_t command;
        command.utime = drc::Clock::instance()->getCurrentTime();
        command.map_id = -1;
        command.command = drc::map_command_t::CLEAR;
        self->mLcm->publish("MAP_COMMAND", &command);
      }

      else if (self->mMapCommand == MAP_COMMAND_SCAN_HIGH_RES) {
        drc::map_macro_t macro;
        macro.utime = drc::Clock::instance()->getCurrentTime();
        macro.command = drc::map_macro_t::CREATE_DENSE_MAP;
        self->mLcm->publish("MAP_MACRO", &macro);
      }

      else {
        std::cout << "WARNING: BAD MAP COMMAND TYPE" << std::endl;
      }
    }

    if (!strcmp(iName, PARAM_DATA_REQUEST) && (self->mBoxValid)) {
      maps::ViewBase::Spec spec;
      spec.mMapId = -1;
      spec.mViewId = -1;
      spec.mActive = true;
      switch(self->mRequestType) {
      case REQUEST_TYPE_OCTREE:
        spec.mType = maps::ViewBase::TypeOctree;
        break;
      case REQUEST_TYPE_CLOUD:
        spec.mType = maps::ViewBase::TypePointCloud;
        break;
      case REQUEST_TYPE_RANGE:
        spec.mType = maps::ViewBase::TypeRangeImage;
        break;
      default:
        return;
      }
      spec.mResolution = self->mRequestResolution;
      spec.mFrequency = self->mRequestFrequency;
      spec.mTimeMin = -1;
      spec.mTimeMax = -1;
      spec.mRelativeTime = false;
      if (self->mRequestTimeWindow > 1e-3) {
        spec.mTimeMin = -self->mRequestTimeWindow*1e6;
        spec.mTimeMax = 0;
        spec.mRelativeTime = true;
      }
      spec.mClipPlanes = self->mFrustum.mPlanes;
      spec.mRelativeLocation = false;
      spec.mRelativeLocation = self->mRequestRelativeLocation;
      if (spec.mRelativeLocation) {
        BotTrans trans;
        bot_frames_get_trans(self->mBotFrames, "head", "local", &trans);
        Eigen::Vector3f pos(trans.trans_vec[0], trans.trans_vec[1],
                            trans.trans_vec[2]);
        Eigen::Quaternionf q(trans.rot_quat[0], trans.rot_quat[1],
                             trans.rot_quat[2], trans.rot_quat[3]);
        Eigen::Matrix3f rotMatx = q.toRotationMatrix();
        Eigen::Vector3f heading = rotMatx.col(0);
        float theta = atan2(heading[1],heading[0]);
        Eigen::Matrix4f planeTransform = Eigen::Matrix4f::Identity();
        planeTransform.topRightCorner<3,1>() = pos;
        Eigen::Matrix3f rotation;
        rotation = Eigen::AngleAxisf(theta, Eigen::Vector3f::UnitZ());
        planeTransform.topLeftCorner<3,3>() = rotation;
        planeTransform = planeTransform.transpose();
        for (size_t i = 0; i < spec.mClipPlanes.size(); ++i) {
          spec.mClipPlanes[i] = planeTransform*spec.mClipPlanes[i];
        }
      }
      self->mViewClient.request(spec);
      bot_gtk_param_widget_set_enum(self->mWidget, PARAM_INPUT_MODE,
                                    INPUT_MODE_CAMERA);
      self->mBoxValid = false;
    }
  }

  static void draw(BotViewer *iViewer, BotRenderer *iRenderer) {
    RendererMaps *self = (RendererMaps*) iRenderer->user;

    // save state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();

    // set rendering flags
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); 
    glEnable(GL_RESCALE_NORMAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // clouds
    std::vector<maps::ViewClient::ViewPtr> views =
      self->mViewClient.getAllViews();
    for (size_t v = 0; v < views.size(); ++v) {
      maps::ViewClient::ViewPtr view = views[v];

      // try to find ancillary data for this view; add if it doesn't exist
      int64_t id = view->getId();
      self->addViewMetaData(id);
      ViewMetaData data = self->mViewData[id];

      // draw box
      if (data.mFrustum.mPlanes.size() > 0) {
        BotTrans trans;
        bot_frames_get_trans_with_utime(self->mBotFrames,
                                        "head", "local",
                                        view->getUpdateTime(), &trans);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        if (data.mRelative) {
          glTranslatef(trans.trans_vec[0], trans.trans_vec[1],
                       trans.trans_vec[2]);
        }
        self->drawFrustum(data.mFrustum, data.mColor);
        glPopMatrix();
      }

      if (!data.mVisible) {
        continue;
      }

      // see if point cloud has any data
      maps::PointCloud::Ptr cloud = view->getAsPointCloud();
      if (cloud->size() == 0) {
        continue;
      }


      //
      // draw point cloud
      //

      self->mMeshRenderer->setColor(data.mColor[0], data.mColor[1],
                                    data.mColor[2]);
      switch(self->mColorMode) {
      case COLOR_MODE_CAMERA:
        self->mMeshRenderer->setColorMode(maps::MeshRenderer::ColorModeCamera);
        break;
      case COLOR_MODE_HEIGHT:
        self->mMeshRenderer->setColorMode(maps::MeshRenderer::ColorModeHeight);
        break;
      case COLOR_MODE_RANGE:
        {
          BotTrans trans;
          bot_frames_get_trans(self->mBotFrames, "head", "local", &trans);
          Eigen::Vector3f pos(trans.trans_vec[0], trans.trans_vec[1],
                              trans.trans_vec[2]);
          self->mMeshRenderer->setRangeOrigin(pos);
          self->mMeshRenderer->setColorMode(maps::MeshRenderer::ColorModeRange);
        }
        break;
      case COLOR_MODE_SOLID:
      default:
        self->mMeshRenderer->setColorMode(maps::MeshRenderer::ColorModeFlat);
        break;
      }

      if (self->mMeshMode == MESH_MODE_POINTS) {
        std::vector<Eigen::Vector3f> vertices(cloud->size());
        for (size_t k = 0; k < cloud->size(); ++k) {
          vertices[k] = Eigen::Vector3f((*cloud)[k].x, (*cloud)[k].y,
                                        (*cloud)[k].z);
        }
        std::vector<Eigen::Vector3i> faces;
        self->mMeshRenderer->setMeshMode(maps::MeshRenderer::MeshModePoints);
        self->mMeshRenderer->setData(vertices, faces);
      }
      /* TODO
      else if (self->mMeshMode == MESH_MODE_SURFELS) {
        if (data.mSurfels != NULL) {
          self->mMeshRenderer->setMeshMode(maps::MeshRenderer::MeshModeFilled);
          self->mMeshRenderer->setData(data.mSurfels->mVertices,
                                       data.mSurfels->mFaces);
        }
      }
      else if (data.mMesh != NULL) {
        if (self->mMeshMode == MESH_MODE_WIREFRAME) {
          self->mMeshRenderer->
            setMeshMode(maps::MeshRenderer::MeshModeWireframe);
        }
        else {
          self->mMeshRenderer->
            setMeshMode(maps::MeshRenderer::MeshModeFilled);
        }
        self->mMeshRenderer->setData(data.mMesh->mVertices, data.mMesh->mFaces);
      }
      */
      self->mMeshRenderer->draw();
    }

    // draw drag rectangle
    if (self->mDragging && self->mInputMode == INPUT_MODE_RECT) {
      int width = GTK_WIDGET(iViewer->gl_area)->allocation.width;
      int height = GTK_WIDGET(iViewer->gl_area)->allocation.height;
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0, width, height, 0, -1, 1);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();
      glDisable(GL_DEPTH_TEST);

      glColor3f(0,0,1);
      glBegin(GL_LINE_LOOP);
      glVertex2f(self->mDragPoint1[0], self->mDragPoint1[1]);
      glVertex2f(self->mDragPoint2[0], self->mDragPoint1[1]);
      glVertex2f(self->mDragPoint2[0], self->mDragPoint2[1]);
      glVertex2f(self->mDragPoint1[0], self->mDragPoint2[1]);
      glEnd();

      glEnable(GL_DEPTH_TEST);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
    }

    if (self->mBoxValid) {
      self->updateFrontAndBackPlanes(self->mFrustum);
      self->drawFrustum(self->mFrustum, Eigen::Vector3f(0,0,1));
    }

    // restore state
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopClientAttrib();
    glPopAttrib();
  }

  void removeViewMetaData(const int64_t iId) {
    mViewData.erase(iId);
    WidgetMap::iterator item = mViewWidgets.find(iId);
    if (item != mViewWidgets.end()) {
      gtk_widget_destroy(item->second.mBox);
      mViewWidgets.erase(item);
    }
  }

  void addViewMetaData(const int64_t iId) {
    bool added = false;
    DataMap::const_iterator item = mViewData.find(iId);
    ViewMetaData data;
    if (item == mViewData.end()) {
      added = true;
      data.mId = iId;
      data.mVisible = true;
      data.mRelative = false;
      if (data.mId != 1) {
        data.mColor = Eigen::Vector3f((double)rand()/RAND_MAX,
                                      (double)rand()/RAND_MAX,
                                      (double)rand()/RAND_MAX);
      }
      else {
        data.mColor = Eigen::Vector3f(1,0,0);
      }
      mViewData[iId] = data;
    }

    maps::ViewBase::Spec spec;
    if (mViewClient.getSpec(iId, spec)) {
      const std::vector<Eigen::Vector4f>& planes = spec.mClipPlanes;
      if (planes.size() != data.mFrustum.mPlanes.size()) {
        mViewData[iId].mFrustum.mPlanes = spec.mClipPlanes;
        mViewData[iId].mRelative = spec.mRelativeLocation;
      }
    }


    if (added) {
      char name[256];
      sprintf(name, "%ld", iId);

      GtkBox *hb = GTK_BOX(gtk_hbox_new (FALSE, 0));

      ViewWidget viewWidget;
      viewWidget.mId = iId;
      viewWidget.mRenderer = this;
      viewWidget.mBox = GTK_WIDGET(hb);
      mViewWidgets[iId] = viewWidget;

      GtkWidget* cb = gtk_toggle_button_new_with_label("                ");
      GdkColor color, colorDim;
      color.red =   data.mColor[0]*65535;
      color.green = data.mColor[1]*65535;
      color.blue =  data.mColor[2]*65535;
      colorDim.red =   (data.mColor[0]+1)/2*65535;
      colorDim.green = (data.mColor[1]+1)/2*65535;
      colorDim.blue =  (data.mColor[2]+1)/2*65535;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), TRUE);
      gtk_widget_modify_bg(GTK_WIDGET(cb), GTK_STATE_ACTIVE, &color);
      gtk_widget_modify_bg(GTK_WIDGET(cb), GTK_STATE_NORMAL, &colorDim);
      gtk_box_pack_start(GTK_BOX(hb), cb, FALSE, FALSE, 0);
      g_signal_connect(G_OBJECT(cb), "toggled",
                       G_CALLBACK(&RendererMaps::onToggleButton),
                       &mViewWidgets[iId]);

      if (iId != 1) {
        GtkWidget* cancelButton = gtk_button_new_with_label("X");
        gtk_box_pack_start(GTK_BOX(hb), cancelButton, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(cancelButton), "clicked",
                         G_CALLBACK(&RendererMaps::onCancelButton),
                         &mViewWidgets[iId]);
      }

      gtk_widget_show_all(GTK_WIDGET(hb));
      gtk_box_pack_start(GTK_BOX(mViewListWidget), GTK_WIDGET(hb),
                         TRUE, TRUE, 0);
    }
  }

  static void onToggleButton(GtkWidget *iButton, ViewWidget* data) {
    bool checked = gtk_toggle_button_get_active((GtkToggleButton*)iButton);
    int64_t id = data->mId;
    DataMap::iterator item = data->mRenderer->mViewData.find(id);
    if (item != data->mRenderer->mViewData.end()) {
      item->second.mVisible = checked;
      bot_viewer_request_redraw (data->mRenderer->mViewer);
    }
  }

  static void onCancelButton(GtkWidget *iButton, ViewWidget* data) {
    int64_t id = data->mId;
    DataMap::iterator item = data->mRenderer->mViewData.find(id);
    if (item != data->mRenderer->mViewData.end()) {
      maps::ViewBase::Spec spec;
      spec.mMapId = 0;
      spec.mViewId = id;
      spec.mResolution = spec.mFrequency = 0;
      spec.mTimeMin = spec.mTimeMax = 0;
      spec.mActive = false;
      spec.mType = maps::ViewBase::TypePointCloud;
      data->mRenderer->mViewClient.request(spec);
      bot_viewer_request_redraw (data->mRenderer->mViewer);
    }
    // TODO: guard thread safety for access to data pointer
  }

  static void drawFrustum(const Frustum& iFrustum,
                          const Eigen::Vector3f& iColor) {
    std::vector<Eigen::Vector3f> vertices;
    std::vector<std::vector<int> > faces;
    maps::Utils::polyhedronFromPlanes(iFrustum.mPlanes, vertices, faces);
    glColor3f(iColor[0], iColor[1], iColor[2]);
    glLineWidth(3);
    for (size_t i = 0; i < faces.size(); ++i) {
      glBegin(GL_LINE_LOOP);
      for (size_t j = 0; j < faces[i].size(); ++j) {
        Eigen::Vector3f pt = vertices[faces[i][j]];
        glVertex3f(pt[0], pt[1], pt[2]);
      }
      glEnd();
    }
  }

  static Eigen::Vector4f computePlane(Eigen::Vector3f& p1, Eigen::Vector3f& p2,
                                      Eigen::Vector3f& p3) {
    Eigen::Vector4f plane;
    Eigen::Vector3f d1 = p2-p1;
    Eigen::Vector3f d2 = p3-p1;
    Eigen::Vector3f normal = d1.cross(d2);
    normal.normalize();
    plane.head<3>() = normal;
    plane[3] = -normal.dot(p1);
    return plane;
  }

  void updateFrontAndBackPlanes(Frustum& ioFrustum) {
    Eigen::Vector4f plane;
    Eigen::Vector3f pt;
    plane.head<3>() = ioFrustum.mDir;
    plane[3] = -ioFrustum.mDir.dot(ioFrustum.mPos +
                                   ioFrustum.mDir*ioFrustum.mNear);
    ioFrustum.mPlanes[4] = plane;
    plane = -plane;
    plane[3] = ioFrustum.mDir.dot(ioFrustum.mPos +
                                  ioFrustum.mDir*ioFrustum.mFar);
    ioFrustum.mPlanes[5] = plane;
  }

  static Eigen::Vector3f intersect(const Eigen::Vector3f& iOrigin,
                                   const Eigen::Vector3f& iDir,
                                   const Eigen::Vector4f& iPlane) {
    Eigen::Vector3f normal = iPlane.head<3>();
    double t = -(iPlane[3] + normal.dot(iOrigin)) / normal.dot(iDir);
    return (iOrigin + iDir*t);
  }


  //
  // event stuff
  //

  static int keyPress(BotViewer* iViewer, BotEventHandler* iHandler, 
                      const GdkEventKey* iEvent) {
    RendererMaps* self = (RendererMaps*)iHandler->user;
    if ((self->mInputMode != INPUT_MODE_CAMERA) &&
        (iEvent->keyval == GDK_Escape)) {
      bot_gtk_param_widget_set_enum(self->mWidget, PARAM_INPUT_MODE,
                                    INPUT_MODE_CAMERA);
      return 0;
    }
    return 0;
  }

  static int mousePress(BotViewer* iViewer, BotEventHandler* iHandler,
                        const double iRayOrg[3], const double iRayDir[3],
                        const GdkEventButton* iEvent) {
    RendererMaps* self = (RendererMaps*)iHandler->user;

    if (self->mInputMode == INPUT_MODE_CAMERA) {
    }
    else if (self->mInputMode == INPUT_MODE_RECT) {
      if (iEvent->button == 1) {
        self->mDragging = true;
        self->mBoxValid = false;
        self->mDragPoint1[0] = iEvent->x;
        self->mDragPoint1[1] = iEvent->y;
        self->mDragPoint2 = self->mDragPoint1;
        bot_viewer_request_redraw(self->mViewer);
        return 1;
      }
      else {}
    }
    else if (self->mInputMode == INPUT_MODE_DEPTH) {
      if (self->mBoxValid) {
        if ((iEvent->button == 1) || (iEvent->button == 3)) {
          self->mDragging = true;
          self->mWhichButton = iEvent->button;
          self->mDragPoint1[0] = iEvent->x;
          self->mDragPoint1[1] = iEvent->y;
          self->mDragPoint2 = self->mDragPoint1;
          self->mBaseValue = (iEvent->button == 1) ? self->mFrustum.mNear :
            self->mFrustum.mFar;
          bot_viewer_request_redraw(self->mViewer);
          return 1;
        }
        else {}
      }
    }
    else {}
    bot_viewer_request_redraw(self->mViewer);
    return 0;
  }

  static int mouseRelease(BotViewer* iViewer, BotEventHandler* iHandler,
                          const double iRayOrg[3], const double iRayDir[3],
                          const GdkEventButton* iEvent) {
    RendererMaps* self = (RendererMaps*)iHandler->user;

    if (self->mInputMode == INPUT_MODE_CAMERA) {
    }
    else if (self->mInputMode == INPUT_MODE_RECT) {
      if (iEvent->button == 1) {
        self->mDragging = false;

        if ((self->mDragPoint2-self->mDragPoint1).norm() < 1) {
          bot_viewer_request_redraw(self->mViewer);
          return 0;
        }

        // compute min and max extents of drag
        Eigen::Vector2f dragPoint1, dragPoint2;
        dragPoint1[0] = std::min(self->mDragPoint1[0], self->mDragPoint2[0]);
        dragPoint1[1] = std::min(self->mDragPoint1[1], self->mDragPoint2[1]);
        dragPoint2[0] = std::max(self->mDragPoint1[0], self->mDragPoint2[0]);
        dragPoint2[1] = std::max(self->mDragPoint1[1], self->mDragPoint2[1]);
        
        // get view info
        GLdouble modelViewGl[16];
        glGetDoublev(GL_MODELVIEW_MATRIX, modelViewGl);
        GLdouble projGl[16];
        glGetDoublev(GL_PROJECTION_MATRIX, projGl);
        GLint viewportGl[4];
        glGetIntegerv(GL_VIEWPORT, viewportGl);

        Eigen::Isometry3f modelView;
        Eigen::Matrix4f proj;
        for (int i = 0; i < 4; ++i) {
          for (int j = 0; j < 4; ++j) {
            modelView(i,j) = modelViewGl[4*j+i];
            proj(i,j) = projGl[4*j+i];
          }
        }

        // eye position and look direction
        Eigen::Vector3f pos =
          -modelView.linear().inverse()*modelView.translation();
        Eigen::Vector3f dir = -modelView.linear().block<1,3>(2,0);
        dir.normalize();
        self->mFrustum.mPos = pos;
        self->mFrustum.mDir = dir;

        // compute four corner rays
        int height = GTK_WIDGET(iViewer->gl_area)->allocation.height;
        double u1(dragPoint1[0]), v1(height-dragPoint1[1]);
        double u2(dragPoint2[0]), v2(height-dragPoint2[1]);
        double x,y,z;
        gluUnProject(u1,v1,0,modelViewGl,projGl,viewportGl,&x,&y,&z);
        Eigen::Vector3f p1(x,y,z);
        gluUnProject(u2,v1,0,modelViewGl,projGl,viewportGl,&x,&y,&z);
        Eigen::Vector3f p2(x,y,z);
        gluUnProject(u2,v2,0,modelViewGl,projGl,viewportGl,&x,&y,&z);
        Eigen::Vector3f p3(x,y,z);
        gluUnProject(u1,v2,0,modelViewGl,projGl,viewportGl,&x,&y,&z);
        Eigen::Vector3f p4(x,y,z);

        self->mFrustum.mNear = 5;
        self->mFrustum.mFar = 10;

        // first four planes
        self->mFrustum.mPlanes.resize(6);
        self->mFrustum.mPlanes[0] = self->computePlane(pos, p1, p2);
        self->mFrustum.mPlanes[1] = self->computePlane(pos, p2, p3);
        self->mFrustum.mPlanes[2] = self->computePlane(pos, p3, p4);
        self->mFrustum.mPlanes[3] = self->computePlane(pos, p4, p1);

        // front and back
        self->updateFrontAndBackPlanes(self->mFrustum);

        self->mBoxValid = true;

        bot_viewer_request_redraw(self->mViewer);
        return 1;
      }
      else {}
    }
    else if (self->mInputMode == INPUT_MODE_DEPTH) {
      self->mDragging = false;
    }
    else {}
    bot_viewer_request_redraw(self->mViewer);
    return 0;
  }

  static int mouseMotion(BotViewer* iViewer, BotEventHandler* iHandler,
                         const double iRayOrg[3], const double iRayDir[3],
                         const GdkEventMotion* iEvent) {
    RendererMaps* self = (RendererMaps*)iHandler->user;

    if (self->mInputMode == INPUT_MODE_CAMERA) {
    }
    else if (self->mInputMode == INPUT_MODE_RECT) {
      if (self->mDragging) {
        self->mDragPoint2[0] = iEvent->x;
        self->mDragPoint2[1] = iEvent->y;
        bot_viewer_request_redraw(self->mViewer);
        return 1;
      }
      else {}
    }
    else if (self->mInputMode == INPUT_MODE_DEPTH) {
      if (self->mBoxValid && self->mDragging) {
        self->mDragPoint2[0] = iEvent->x;
        self->mDragPoint2[1] = iEvent->y;
        float dist = self->mDragPoint2[1]-self->mDragPoint1[1];
        float scaledDist = dist/100;
        if (self->mWhichButton == 1) {
          self->mFrustum.mNear = std::max(0.0f, self->mBaseValue+scaledDist);
          bot_viewer_request_redraw(self->mViewer);
          return 1;
        }
        else if (self->mWhichButton == 3) {
          self->mFrustum.mFar = std::max(0.0f, self->mBaseValue+scaledDist);
          bot_viewer_request_redraw(self->mViewer);
          return 1;
        }
        else {}
      }
      else {}
    }
    else {}
    bot_viewer_request_redraw(self->mViewer);
    return 0;
  }

};


// TODO: move these inside

static void
maps_renderer_free (BotRenderer *renderer) {
  RendererMaps *self = (RendererMaps*) renderer;
  delete self;
}

static void
on_load_preferences (BotViewer *viewer, GKeyFile *keyfile, void *user_data) {
  RendererMaps *self = (RendererMaps*) user_data;
  bot_gtk_param_widget_load_from_key_file (self->mWidget, keyfile,
                                           RENDERER_NAME);
}

static void
on_save_preferences (BotViewer *viewer, GKeyFile *keyfile, void *user_data) {
    RendererMaps *self = (RendererMaps*)user_data;
    bot_gtk_param_widget_save_to_key_file (self->mWidget, keyfile,
                                           RENDERER_NAME);
}

void
maps_add_renderer_to_viewer(BotViewer* viewer,
                            int priority,
                            lcm_t* lcm,
                            BotParam* param,
                            BotFrames* frames) {
  RendererMaps *self = new RendererMaps();
  self->mViewer = viewer;
  self->mRenderer.draw = RendererMaps::draw;
  self->mRenderer.destroy = maps_renderer_free;
  self->mRenderer.name = (char*)RENDERER_NAME;
  self->mRenderer.user = self;
  self->mRenderer.enabled = 1;
  self->mRenderer.widget = gtk_alignment_new (0, 0.5, 1.0, 0);
  self->mLcm.reset(new lcm::LCM(lcm));
  self->mBotParam = param;
  self->mBotFrames = frames;

  drc::Clock::instance()->setLcm(self->mLcm);
  self->mMeshRenderer->setBotParam(self->mBotParam);
  self->mMeshRenderer->setLcm(self->mLcm);

  // events
  BotEventHandler* handler = &self->mEventHandler;
  memset(handler, 0, sizeof(BotEventHandler));
  handler->name = (char*)RENDERER_NAME;
  handler->enabled = 1;
  handler->key_press = RendererMaps::keyPress;
  handler->mouse_press = RendererMaps::mousePress;
  handler->mouse_release = RendererMaps::mouseRelease;
  handler->mouse_motion = RendererMaps::mouseMotion;
  handler->user = self;
  bot_viewer_add_event_handler(viewer, &self->mEventHandler, priority);


  self->mWidget = BOT_GTK_PARAM_WIDGET (bot_gtk_param_widget_new ());
  gtk_container_add (GTK_CONTAINER (self->mRenderer.widget),
                     GTK_WIDGET(self->mWidget));
  gtk_widget_show (GTK_WIDGET (self->mWidget));

  bot_gtk_param_widget_add_separator(self->mWidget, "appearance");
  bot_gtk_param_widget_add_enum(self->mWidget, PARAM_COLOR_MODE,
                                BOT_GTK_PARAM_WIDGET_MENU, COLOR_MODE_SOLID,
                                "Solid Color", COLOR_MODE_SOLID,
                                "Height Pseudocolor", COLOR_MODE_HEIGHT,
                                "Range Pseudocolor", COLOR_MODE_RANGE,
                                "Camera Texture", COLOR_MODE_CAMERA, NULL);
  bot_gtk_param_widget_add_enum(self->mWidget, PARAM_MESH_MODE,
                                BOT_GTK_PARAM_WIDGET_MENU, MESH_MODE_POINTS,
                                "Point Cloud", MESH_MODE_POINTS,
                                "Surfels", MESH_MODE_SURFELS,
                                "Wireframe", MESH_MODE_WIREFRAME,
                                "Filled", MESH_MODE_FILLED, NULL);
  bot_gtk_param_widget_add_double (self->mWidget, PARAM_POINT_SIZE,
                                   BOT_GTK_PARAM_WIDGET_SLIDER,
                                   0, 10, 0.1, 3);

  bot_gtk_param_widget_add_separator(self->mWidget, "view creation");

  bot_gtk_param_widget_add_enum(self->mWidget, PARAM_INPUT_MODE,
                                BOT_GTK_PARAM_WIDGET_MENU, INPUT_MODE_CAMERA,
                                "Move View", INPUT_MODE_CAMERA,
                                "Drag Rect", INPUT_MODE_RECT,
                                "Set Depth", INPUT_MODE_DEPTH, NULL);

  bot_gtk_param_widget_add_enum(self->mWidget, PARAM_REQUEST_TYPE,
                                BOT_GTK_PARAM_WIDGET_MENU, REQUEST_TYPE_CLOUD,
                                "Cloud", REQUEST_TYPE_CLOUD,
                                "Octree", REQUEST_TYPE_OCTREE, NULL);

  bot_gtk_param_widget_add_double(self->mWidget, PARAM_REQUEST_FREQ,
                                  BOT_GTK_PARAM_WIDGET_SPINBOX,
                                  0, 5, 0.1, 1);

  bot_gtk_param_widget_add_double(self->mWidget, PARAM_REQUEST_RES,
                                  BOT_GTK_PARAM_WIDGET_SPINBOX,
                                  0.01, 1, 0.01, 0.05);

  bot_gtk_param_widget_add_double(self->mWidget, PARAM_REQUEST_TIME,
                                  BOT_GTK_PARAM_WIDGET_SPINBOX,
                                  0, 10, 0.1, 0);

  bot_gtk_param_widget_add_booleans(self->mWidget,
                                    BOT_GTK_PARAM_WIDGET_CHECKBOX,
                                    PARAM_REQUEST_RELATIVE, 0, NULL);

  bot_gtk_param_widget_add_buttons(self->mWidget, PARAM_DATA_REQUEST, NULL);

  bot_gtk_param_widget_add_separator(self->mWidget, "macro commands");

  bot_gtk_param_widget_add_enum(self->mWidget, PARAM_MAP_COMMAND_TYPE,
                                BOT_GTK_PARAM_WIDGET_MENU, MAP_COMMAND_CLEAR,
                                "Clear Map", MAP_COMMAND_CLEAR,
                                "High-Res Scan", MAP_COMMAND_SCAN_HIGH_RES,
                                NULL);
  bot_gtk_param_widget_add_buttons(self->mWidget, PARAM_MAP_COMMAND, NULL);

  bot_gtk_param_widget_add_separator(self->mWidget, "view list");
  self->mViewListWidget = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (self->mWidget), self->mViewListWidget);
  gtk_widget_show (self->mViewListWidget);
  
  g_signal_connect (G_OBJECT (self->mWidget), "changed",
                    G_CALLBACK (RendererMaps::onParamWidgetChanged),
                    self);


  g_signal_connect (G_OBJECT (viewer), "load-preferences",
                    G_CALLBACK (on_load_preferences), self);
  g_signal_connect (G_OBJECT (viewer), "save-preferences",
                    G_CALLBACK (on_save_preferences), self);

  self->mViewClient.setLcm(self->mLcm);
  self->mViewClient.setOctreeChannel("MAP_OCTREE");
  self->mViewClient.setCloudChannel("MAP_CLOUD");
  self->mViewClientListener.mRenderer = self;
  self->mViewClientListener.mInitialized = false;
  self->mViewClient.addListener(&self->mViewClientListener);
  self->mViewClient.start();

  std::cout << "Finished Setting Up Maps Renderer" << std::endl;

  bot_viewer_add_renderer(viewer, &self->mRenderer, priority);
}
