#ifndef _ROS_ar_track_alvar_msgs_AlvarMarkers_h
#define _ROS_ar_track_alvar_msgs_AlvarMarkers_h

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ros/msg.h"
#include "std_msgs/Header.h"
#include "ar_track_alvar_msgs/AlvarMarker.h"

namespace ar_track_alvar_msgs
{

  class AlvarMarkers : public ros::Msg
  {
    public:
      typedef std_msgs::Header _header_type;
      _header_type header;
      uint32_t markers_length;
      typedef ar_track_alvar_msgs::AlvarMarker _markers_type;
      _markers_type st_markers;
      _markers_type * markers;

    AlvarMarkers():
      header(),
      markers_length(0), markers(NULL)
    {
    }

    virtual int serialize(unsigned char *outbuffer) const
    {
      int offset = 0;
      offset += this->header.serialize(outbuffer + offset);
      *(outbuffer + offset + 0) = (this->markers_length >> (8 * 0)) & 0xFF;
      *(outbuffer + offset + 1) = (this->markers_length >> (8 * 1)) & 0xFF;
      *(outbuffer + offset + 2) = (this->markers_length >> (8 * 2)) & 0xFF;
      *(outbuffer + offset + 3) = (this->markers_length >> (8 * 3)) & 0xFF;
      offset += sizeof(this->markers_length);
      for( uint32_t i = 0; i < markers_length; i++){
      offset += this->markers[i].serialize(outbuffer + offset);
      }
      return offset;
    }

    virtual int deserialize(unsigned char *inbuffer)
    {
      int offset = 0;
      offset += this->header.deserialize(inbuffer + offset);
      uint32_t markers_lengthT = ((uint32_t) (*(inbuffer + offset))); 
      markers_lengthT |= ((uint32_t) (*(inbuffer + offset + 1))) << (8 * 1); 
      markers_lengthT |= ((uint32_t) (*(inbuffer + offset + 2))) << (8 * 2); 
      markers_lengthT |= ((uint32_t) (*(inbuffer + offset + 3))) << (8 * 3); 
      offset += sizeof(this->markers_length);
      if(markers_lengthT > markers_length)
        this->markers = (ar_track_alvar_msgs::AlvarMarker*)realloc(this->markers, markers_lengthT * sizeof(ar_track_alvar_msgs::AlvarMarker));
      markers_length = markers_lengthT;
      for( uint32_t i = 0; i < markers_length; i++){
      offset += this->st_markers.deserialize(inbuffer + offset);
        memcpy( &(this->markers[i]), &(this->st_markers), sizeof(ar_track_alvar_msgs::AlvarMarker));
      }
     return offset;
    }

    const char * getType(){ return "ar_track_alvar_msgs/AlvarMarkers"; };
    const char * getMD5(){ return "943fe17bfb0b4ea7890368d0b25ad0ad"; };

  };

}
#endif