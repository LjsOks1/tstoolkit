#include <gst/gst.h>
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#include "config.h"
#include "pmt.h"

extern Settings *conf;

/****************************************************************************** 
 * process_pmt calculates the outgoing PMT from the incoming one and the config file
  
 *******************************************************************************/

gboolean process_pmt(ProgramData *data)
{
    PMT *pmt=data->pmt;
    GstMpegtsSection *section_in = NULL;
    GstMpegtsSection *section_out = NULL;
    GstMpegtsPMT *pmt_in = NULL;
    GstMpegtsPMTStream *scte_stream = NULL ;

    
    GST_LOG("Processing PMT...");

    section_in=gst_mpegts_section_new(pmt->pmt_pid, pmt->raw_section_in,pmt->raw_section_in_length);
     
    if(section_in!=NULL) {
      pmt->pmt_version_in=section_in->version_number;      
      section_in->version_number=g_random_int_range(0,16);
      pmt_in=gst_mpegts_section_get_pmt(section_in);
      if(data->ait->ait_defined) {
        for(int i=0;i<pmt_in->streams->len;i++) {
          if(((GstMpegtsPMTStream*)g_ptr_array_index(pmt_in->streams,i))->pid==data->ait->ait_pid) {
            g_ptr_array_remove_index(pmt_in->streams,i);
          }
        }
      }
      if(data->scte35->scte35_defined==TRUE) {
        GstMpegtsDescriptor *registration_descriptor,*stream_identifier_descriptor;;
        guint8 scte_format_identifier[]={0x43, 0x55, 0x45, 0x49};
        guint8 component_tag=0;
        registration_descriptor=gst_mpegts_descriptor_from_custom(0x05,scte_format_identifier,4);
        stream_identifier_descriptor=gst_mpegts_descriptor_from_custom(0x52,&component_tag,1);
        g_ptr_array_add(pmt_in->descriptors,registration_descriptor);
        scte_stream=gst_mpegts_pmt_stream_new();
        scte_stream->stream_type=GST_MPEGTS_STREAM_TYPE_SCTE_SIT;
        scte_stream->pid=data->scte35->scte35_pid;
        g_ptr_array_add(scte_stream->descriptors,stream_identifier_descriptor);
        g_ptr_array_add(pmt_in->streams,scte_stream);
      }
      if(data->ait->ait_defined /* && (g_strcmp0(data->ait->ait_stream,"new")==0)*/) {
          GstMpegtsPMTStream *ait_stream =NULL;
          GstMpegtsDescriptor *application_signalling;
          guint8 version;
          guint8 app_type[]={0x00,0x10,0xe2};
          version=g_random_int_range(0,31);
//          app_type[2]=version;
          application_signalling=gst_mpegts_descriptor_from_custom(0x6f,NULL,0);
//          application_signalling=gst_mpegts_descriptor_from_custom(0x6f,app_type,3);
          ait_stream=gst_mpegts_pmt_stream_new();
          ait_stream->stream_type=0x05; //MPEG-2 Private section
          ait_stream->pid=data->ait->ait_pid;
          g_ptr_array_add(ait_stream->descriptors,application_signalling);
          g_ptr_array_add(pmt_in->streams,ait_stream);
      }
      section_out=gst_mpegts_section_from_pmt(pmt_in,pmt->pmt_pid);
      if(pmt->raw_section_out!=NULL) {
         GST_ERROR("PMT raw output section should be NULL at this stage.");
         exit(-1);
      }
      section_out->version_number=g_random_int_range(0,16);
      pmt->raw_section_out=gst_mpegts_section_packetize(section_out,(gsize*)&pmt->raw_section_out_length);
      pmt->raw_section_out_start_index=0;
      return TRUE;
    } else {
        GST_ERROR("Couldn't create PMT section from incoming packets.");
    }
 
    return FALSE;
}


/***************************************************************************** 
 * Creates the next TS packet from the PMT section raw data and appends it 
 * to the output packet_array
 *  
******************************************************************************/
gboolean get_next_pmt_packet(GArray *packet_array,PMT *pmt)
{
  char ff_packet[188];
 
  if(pmt->raw_section_out==NULL) {
    GST_ERROR("Cannot get the next PMT packet if the section is NULL.");
    return FALSE;
  }
  memset(&ff_packet[0],0xff,188);
  ff_packet[0]=0x47;
  ff_packet[1]=((pmt->raw_section_out_start_index==0)<<6) | ((pmt->pmt_pid&0x1fff)>>8);
  ff_packet[2]=pmt->pmt_pid & 0xff;
  ff_packet[3]=((pmt->cc_out++)%16) | 0x10;
  if(pmt->raw_section_out_start_index==0) {
    ff_packet[4]=0;
    if(pmt->raw_section_out_length<181) {
      g_array_append_vals(packet_array,ff_packet,5);
      g_array_append_vals(packet_array,pmt->raw_section_out,pmt->raw_section_out_length);
      g_array_append_vals(packet_array,&ff_packet[5],183-pmt->raw_section_out_length);
      return TRUE;
    } else {
      GST_ERROR("Scenario not covered yet.");
      return FALSE;
    }
  }
  GST_ERROR("Scenario not covered yet. Incoming PMT:%i, outgoing PMT:%i bytes long",
      pmt->raw_section_in_length,pmt->raw_section_out_length);
  return FALSE;
}

/******************************************************************** 
*  Builds the PMT section from the incoming ts packets.
*  If the complete PMT section is available:
*   - check if it is with a new version?
*       - if yes: recalculate the new outgoing PMT based on the new incoming one
*       - if no: just send out the next ts packet from the already modified PMT          
*  If no PMT section is available yet, forward the incoming ts packet.
********************************************************************/
gboolean push_pmt_packet(guint8 *ptr, GArray *packet_array, void *data)
{
  PMT *pmt=((ProgramData*)data)->pmt;
  int adaptation_field_length=0;
//FIXME: Can we have adaptation field as stuffing bytes?????
    if( ((ptr[1] & 0x80) == 0) &&       // Transport error indicator == 0
        ((ptr[3] & 0xc0) == 0) ) {       // Scrambling control == 0
        if((ptr[3] & 0x30)==0x30)  { //Adaptation field used as stuffing. Skiping it.
          adaptation_field_length=ptr[4]+1;
        }
        if((ptr[1] & 0x40) !=0 ) {      // If PUSI==1 process the existing PMT buffer.
            if(ptr[adaptation_field_length+4]!=0) { //Section doesn't start at the start of the TS packet. Cannot handle it now....
              GST_ERROR("New PMT section doesn't start at the beginning of the TS packet. Cannot handle this scenario yet.");
              exit(-1);
            }
            if(ptr[adaptation_field_length+5]!=0x02) { // Not a PMT table. Something is wrong.
              GST_ERROR("Not a PMT table on PMT PID. Exiting....");
              exit(-1);
            }

            if(pmt->raw_section_in!=NULL) { // We have a complete PMT section from th eincoming stream. 
              if(pmt->raw_section_out==NULL) { // Let's parse the section into a PMT structure as we don't have it yet...
                process_pmt(data); //Create the modified raw section out data for the outgoing stream
              }
              if(((pmt->raw_section_in[5] & 0x3e)>>1)!=(((ptr[adaptation_field_length+10]&0x3e)>>1)+0%32) ) {  //We have a new PMT version, delete section buffer plus the in and out PMT structures...
                g_free(pmt->raw_section_in);
                pmt->raw_section_in=NULL;
                g_free(pmt->raw_section_out);
                pmt->raw_section_out=NULL;
              }
            } 
            if(((ptr[1] & 0x40)!=0) && (pmt->raw_section_in==NULL) )   { // PUSI==1 but no section buffer yet. Allocate a new buffer to *pmt_section
              pmt->raw_section_in_length=((ptr[adaptation_field_length+6]&0x0f)<<8 | ptr[adaptation_field_length+7])+3;
              pmt->raw_section_in=g_malloc0(pmt->raw_section_in_length);
              if(pmt->raw_section_in_length<181) { //The PMT section fits into a single TS packet. 
                memcpy(pmt->raw_section_in,&ptr[adaptation_field_length+5],pmt->raw_section_in_length);
                pmt->missing_bytes_from_section_in=0;
              } else { //PMT section continues in the next TS packet. 
                memcpy(pmt->raw_section_in,&ptr[adaptation_field_length+5],180-adaptation_field_length);
                pmt->missing_bytes_from_section_in=pmt->raw_section_in_length-180+adaptation_field_length;
              }
//              pmt->raw_section_in[5]
            }

        } else { //PUSI==0;
          if(pmt->raw_section_in!=NULL) { // append packet to section only if pmt_section buffer already exists...
            if(pmt->missing_bytes_from_section_in<182) { //we don't have the pointer field if PUSI==0; one more bytes can be copied.
              memcpy(&pmt->raw_section_in[pmt->raw_section_in_length-pmt->missing_bytes_from_section_in],&ptr[adaptation_field_length+4],pmt->missing_bytes_from_section_in);
              pmt->missing_bytes_from_section_in=0;
            } else {
              memcpy(&pmt->raw_section_in[pmt->raw_section_in_length-pmt->missing_bytes_from_section_in],&ptr[adaptation_field_length+4],181-adaptation_field_length);
              pmt->missing_bytes_from_section_in-=(181-adaptation_field_length);
            }
          }         
        }
        if(pmt->raw_section_out!=NULL) {
          get_next_pmt_packet(packet_array,pmt);

        }
        else {
          g_array_append_vals(packet_array,ptr,188);
        }
        return TRUE;
    }    
    return FALSE;
}





