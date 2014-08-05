/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redstributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Wim Meeussen */
// Modified from ROS's URDF_DOM library Written by Josh Faust and Wim Meeussen

#include "otdf_parser/otdf_parser.h"
#include <iostream>
#include <fstream>

using namespace otdf;
using namespace std;

void addChildLinkNames(boost::shared_ptr<const BaseEntity> entity, ofstream& os)
{
  
   std::string type = entity->getEntityType();
  if (type == "Link"){
       os << "\"" << entity->name << "\" [label=\"" << entity->name << "\"];" << endl;
    boost::shared_ptr<const Link> downcasted_entity(boost::dynamic_pointer_cast<const Link>(entity)); 
   
  for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
    addChildLinkNames(*child, os);
    
  }
  else if (type == "Bounding_volume"){
  boost::shared_ptr<const Bounding_volume> downcasted_entity(boost::dynamic_pointer_cast<const Bounding_volume>(entity));  
    
  for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
    addChildLinkNames(*child, os);
  }
}

void addChildBoundingVolumeNames(boost::shared_ptr<const BaseEntity> entity, ofstream& os)
{
  
   std::string type = entity->getEntityType();
  if (type == "Link"){
    boost::shared_ptr<const Link> downcasted_entity(boost::dynamic_pointer_cast<const Link>(entity)); 
   
  for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
    addChildBoundingVolumeNames(*child, os);
    
  }
  else if (type == "Bounding_volume"){
   os << "\"" << entity->name << "\" [label=\"" << entity->name << "\"];" << endl;
  boost::shared_ptr<const Bounding_volume> downcasted_entity(boost::dynamic_pointer_cast<const Bounding_volume>(entity));  
    
  for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
    addChildBoundingVolumeNames(*child, os);
  }
}


void printChildJointNames(boost::shared_ptr<const BaseEntity> entity, boost::shared_ptr<const BaseEntity> child, ofstream& os, double &r, double &p, double &y)
{
	  if((child)->getEntityType() == "Link") {
	      boost::shared_ptr<const Link> downcasted_child(boost::dynamic_pointer_cast<const Link>((child))); 
	      (downcasted_child)->parent_joint->parent_to_joint_origin_transform.rotation.getRPY(r,p,y);
	      os << "\"" << entity->name << "\" -> \"" << (downcasted_child)->parent_joint->name 
		<< "\" [label=\"xyz: "
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.x << " " 
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.y << " " 
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.z << " " 
		<< "\\nrpy: " << r << " " << p << " " << y << "\"]" << endl;
	      os << "\"" << (downcasted_child)->parent_joint->name << "\" -> \"" << (downcasted_child)->name << "\"" << endl;
	  }
	  else if ((child)->getEntityType() == "Bounding_volume") {
	      boost::shared_ptr<const Bounding_volume> downcasted_child(boost::dynamic_pointer_cast<const Bounding_volume>((child))); 
	      (downcasted_child)->parent_joint->parent_to_joint_origin_transform.rotation.getRPY(r,p,y);
	      os << "\"" << entity->name << "\" -> \"" << (downcasted_child)->parent_joint->name 
		<< "\" [label=\"xyz: "
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.x << " " 
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.y << " " 
		<< (downcasted_child)->parent_joint->parent_to_joint_origin_transform.position.z << " " 
		<< "\\nrpy: " << r << " " << p << " " << y << "\"]" << endl;
	      os << "\"" << (downcasted_child)->parent_joint->name << "\" -> \"" << (downcasted_child)->name << "\"" << endl;
	  
	  }  

}

void addChildJointNames(boost::shared_ptr<const BaseEntity> entity, ofstream& os)
{
    double r, p, y;
  std::string type = entity->getEntityType();
  if (type == "Link"){
   boost::shared_ptr<const Link> downcasted_entity(boost::dynamic_pointer_cast<const Link>(entity)); 
	
      for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
      {
	printChildJointNames(entity,(*child),os,r,p,y);
	addChildJointNames(*child, os);
      }
  }
  else if (type == "Bounding_volume"){
    boost::shared_ptr<const Bounding_volume> downcasted_entity(boost::dynamic_pointer_cast<const Bounding_volume>(entity));  
      
      for (std::vector<boost::shared_ptr<BaseEntity> >::const_iterator child = downcasted_entity->child_links.begin(); child != downcasted_entity->child_links.end(); child++)
      {
	 printChildJointNames(entity,(*child),os,r,p,y);
	  addChildJointNames(*child, os);
      }
  }
}



void printTree(boost::shared_ptr<const BaseEntity> entity, string file)
{
  std::ofstream os;
  os.open(file.c_str());
  os << "digraph G {" << endl;

  os << "node [shape=box];" << endl;
  addChildLinkNames(entity, os);

  os << "node [shape=box3d,  color=red];" << endl;
  addChildBoundingVolumeNames(entity, os);
  
  os << "node [shape=ellipse, color=blue, fontcolor=blue];" << endl;
  addChildJointNames(entity, os);

  os << "}" << endl;
  os.close();
}



int main(int argc, char** argv)
{
  if (argc != 2){
    std::cerr << "Usage: urdf_to_graphiz input.xml" << std::endl;
    return -1;
  }

  // get the entire file
  std::string xml_string;
  std::fstream xml_file(argv[1], std::fstream::in);
  while ( xml_file.good() )
  {
    std::string line;
    std::getline( xml_file, line);
    xml_string += (line + "\n");
  }
  xml_file.close();

  boost::shared_ptr<ModelInterface> object = parseOTDF(xml_string);
  if (!object){
    std::cerr << "ERROR: Model Parsing the xml failed" << std::endl;
    return -1;
  }
  string output = object->getName();
  
    //   // testing object update for ladder
    //   object->setParam("NO_OF_STEPS", 5);
    //   object->update();
  
  // print entire tree to file
  printTree(object->getRoot(), output+".gv");
  cout << "Created file " << output << ".gv" << endl;

  string command = "dot -Tpdf "+output+".gv  -o "+output+".pdf";
  int temp = system(command.c_str());
  cout << "Created file " << output << ".pdf" << endl;
  return 0;
}

