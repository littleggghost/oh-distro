#ifndef INTERACTABLE_GL_KINEMATIC_BODY_HPP
#define INTERACTABLE_GL_KINEMATIC_BODY_HPP

#include "GlKinematicBody.hpp"
#include <collision/collision_detector.h>
#include <collision/collision_object_sphere.h>
#include <collision/collision_object_cylinder.h>
#include <collision/collision_object_box.h>
namespace visualization_utils {

 
class InteractableGlKinematicBody: public GlKinematicBody 
{

  public:
    std::string _unique_name;
    std::map<std::string,   boost::shared_ptr<collision::Collision_Object> > _collision_object_map;
    
    // create a collision detector class
    // collision::Collision_Detector _collision_detector;
    boost::shared_ptr<collision::Collision_Detector> _collision_detector;
  private:   
   std::string selected_link;
   bool link_selection_enabled;
   bool link_adjustment_enabled;
   bool whole_body_selection_enabled; 
  public:
  //copy Constructors
  InteractableGlKinematicBody( const InteractableGlKinematicBody& other, std::string unique_name); 
  InteractableGlKinematicBody( const GlKinematicBody& other,
                              boost::shared_ptr<collision::Collision_Detector> col_detector, 
                              bool enable_selection,
                              std::string unique_name);
 // Constructors (Reuses code from GLKinematicBody Constructor)
  InteractableGlKinematicBody(std::string urdf_xml_string,
                              boost::shared_ptr<collision::Collision_Detector> col_detector, 
                              bool enable_selection, std::string unique_name);
  InteractableGlKinematicBody (boost::shared_ptr<otdf::ModelInterface> otdf_instance,
                              boost::shared_ptr<collision::Collision_Detector> col_detector,
                              bool enable_selection, std::string unique_name);
  ~InteractableGlKinematicBody();
   void init_urdf_collision_objects();
   void init_otdf_collision_objects();
   void set_state(const drc::robot_state_t &msg); 
   void set_state(const KDL::Frame &T_world_body, const drc::joint_angles_t &msg); 
   void set_state(boost::shared_ptr<otdf::ModelInterface> _otdf_instance);
   void update_urdf_collision_objects(void);
   void update_otdf_collision_objects(void);
  
   // double c[3] = {0.3,0.3,0.3};
   // double alpha = self->alpha;
   void draw_body (float (&c)[3], float alpha)
   {
     
      glColor4f(c[0],c[1],c[2],alpha);
      for(uint i = 0; i < _link_tfs.size(); i++)
      {
        LinkFrameStruct nextTf=_link_tfs[i];
        std::stringstream oss;
        oss << _unique_name << "_"<< _link_tfs[i].name; 
        if((link_selection_enabled)&&(selected_link == oss.str())) {
          
          if((link_adjustment_enabled)&&(is_otdf_instance))
            draw_interactable_markers(_otdf_link_shapes[i],_link_tfs[i]); // draws shapes and adds them to _collision_detector 
          else if((link_adjustment_enabled)&&(!is_otdf_instance)) 
            draw_interactable_markers(_link_shapes[i],_link_tfs[i]);   
            
          glColor4f(0.7,0.1,0.1,alpha);         
        }
        else
           glColor4f(c[0],c[1],c[2],alpha);

        if((whole_body_selection_enabled)&&(selected_link != " ")) {
          glColor4f(0.7,0.1,0.1,alpha); // whole body is selected instead of an individual link
        } 
        GlKinematicBody::draw_link_current_and_future(c,alpha,i,nextTf);  
//        if(is_otdf_instance)
//        {
//         boost::shared_ptr<otdf::Geometry> nextLink = _otdf_link_shapes[i];
//         GlKinematicBody::draw_link(nextLink,nextTf.name,nextTf.frame);
//         
//        }
//        else
//        {     
//         boost::shared_ptr<urdf::Geometry> nextLink = _link_shapes[i];
//         GlKinematicBody::draw_link(nextLink,nextTf.name,nextTf.frame);
//        }
      }
   };
   
   
   void draw_body_in_frame (float (&c)[3], float alpha,const KDL::Frame &T_drawFrame_currentWorldFrame)
   {
     
      glColor4f(c[0],c[1],c[2],alpha);
      for(uint i = 0; i < _link_tfs.size(); i++)
      {
        KDL::Frame T_currentWorldFrame_link,T_drawFrame_link;
        LinkFrameStruct nextTf=_link_tfs[i];
        
        T_currentWorldFrame_link = nextTf.frame;
        T_drawFrame_link = T_drawFrame_currentWorldFrame*T_currentWorldFrame_link;
        nextTf.frame = T_drawFrame_link;

        
        std::stringstream oss;
        oss << _unique_name << "_"<< nextTf.name; 
        if((link_selection_enabled)&&(selected_link == oss.str())) {
          
          if((link_adjustment_enabled)&&(is_otdf_instance))
            draw_interactable_markers(_otdf_link_shapes[i],nextTf); // draws shapes and adds them to _collision_detector 
          else if((link_adjustment_enabled)&&(!is_otdf_instance)) 
            draw_interactable_markers(_link_shapes[i],nextTf);   
            
          glColor4f(0.7,0.1,0.1,alpha);         
        }
        else
           glColor4f(c[0],c[1],c[2],alpha);

        if((whole_body_selection_enabled)&&(selected_link != " ")) {
          glColor4f(0.7,0.1,0.1,alpha); // whole body is selected instead of an individual link
        }
        
        GlKinematicBody::draw_link_current_and_future(c,alpha,i,nextTf);    
      
      }

   };
   
   void draw_interactable_markers(boost::shared_ptr<otdf::Geometry> &_link_shape, const LinkFrameStruct &link_tf);
   void draw_interactable_markers(boost::shared_ptr<urdf::Geometry> &_link_shape,const LinkFrameStruct &link_tf);
   void draw_markers(double (&pos)[3], double (&dim)[3], double markersize); 
   bool get_link_frame(const std::string &link_name, KDL::Frame &T_world_link);

   
   void enable_link_selection(bool value)
   {
       link_selection_enabled = value; 
   };
   
   void highlight_link(std::string &link_name)
   {
       selected_link = link_name; 
   };
   
   void enable_link_adjustment(bool value)
   { 
    link_adjustment_enabled = value;
   };
   
   bool is_link_adjustment_enabled()
   { 
    return link_adjustment_enabled;
   };

   void enable_whole_body_selection(bool value)
   { 
    whole_body_selection_enabled = value;
   };


};

} // end namespace 




#endif //INTERACTABLE_GL_KINEMATIC_BODY_HPP
