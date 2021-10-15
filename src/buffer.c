#include <gst/gst.h>
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#include "config.h"
#include "buffer.h"
#include "x31.h"
#include "pmt.h"

extern Settings *conf;
GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser


gboolean process_packet(guint8* ptr,GArray* packet_array,ProgramData *data) 
{
    int pid = 0;
    if(ptr[0]==0x47) {
        pid=((ptr[1]&0x01f)<<8) + ptr[2];
        if(data->pmt->pmt_defined && (pid==conf->pmt_pid) ) {
            return push_pmt_packet(ptr,packet_array,(void*)data);
        } else if((data->scte35->scte35_defined==TRUE) && (pid== data->scte35->txt_pid)) {
            g_array_append_vals(packet_array,ptr,188);
            return push_txt_packet(ptr,packet_array,data->txt,data->scte35);
        } else if(data->ait->ait_defined && (g_strcmp0(data->ait->ait_stream,"replace")==0) && (pid==data->ait->ait_pid)) {
          data->ait->ts_packet[3]=0x10|((data->ait->cc++)%16); //CC must be increased every time....
          g_array_append_vals(packet_array,data->ait->ts_packet,188);
        } else {
          g_array_append_vals(packet_array,ptr,188);
          g_print(".\r");
        }
        return TRUE;
    }
    GST_ERROR("TS packet hasn't started with the sync byte.");
    return FALSE;    
}

GstBuffer* process_buffer(GstBuffer* buffer_in,ProgramData *data)
{
    GstBuffer *buffer_out;
    GstMapInfo map;
    guint8 *ptr = NULL;
    guint8* tdt_packet=NULL;
    guint8* tot_packet=NULL;
    GstMemory *mem = NULL;
    static gint64 last_ait_sent,last_tdt_sent,last_tot_sent;
    gint64 current_time;
    GArray *packet_array = NULL;
    packet_array=g_array_new(FALSE,TRUE,1);
    if(gst_buffer_map(buffer_in,&map,GST_MAP_READ)) {
        if(map.size%188==0) {
            ptr=map.data;
            while(ptr<map.data+map.size) {
                if(process_packet(ptr,packet_array,data)) {
                    ptr+=188;   
                } else {
                    break;
                }
            }
            if(data->ait->ait_defined && g_strcmp0(data->ait->ait_stream,"new")==0) {
                current_time=g_get_real_time();
                if(current_time>last_ait_sent+250000) {
                    data->ait->ts_packet[3]=0x10|((data->ait->cc++)%16); //CC must be increased every time....
                    g_array_append_vals(packet_array,data->ait->ts_packet,188);
                    last_ait_sent=current_time;
                }
            }
            if(data->tdt->tdt_defined) {
                current_time=g_get_real_time();
                if(current_time>last_tdt_sent+250000) {
                  if(get_tdt_packet(data->tdt,&tdt_packet)) {
                    tdt_packet[3]=0x10|((data->pid20_cc++)%16); //CC must be increased every time....
                    g_array_append_vals(packet_array,tdt_packet,188);
                    free(tdt_packet);
                  }
                  last_tdt_sent=current_time;
                }
            }      
            if(data->tot->tot_defined) {
                current_time=g_get_real_time();
                if(current_time>last_tot_sent+250000) {
                  if(get_tot_packet(data->tot,&tot_packet)) {
                    tot_packet[3]=0x10|((data->pid20_cc++)%16); //CC must be increased every time....
                    g_array_append_vals(packet_array,tot_packet,188);
                    free(tot_packet);
                  }
                  last_tot_sent=current_time;
                }
            }      
            gst_buffer_unmap(buffer_in,&map);
            buffer_out=gst_buffer_new();
            gst_buffer_copy_into(buffer_out,buffer_in,GST_BUFFER_COPY_TIMESTAMPS,0,-1);
            mem=gst_allocator_alloc(NULL,packet_array->len,NULL);
            gst_buffer_append_memory(buffer_out,mem);
            gst_buffer_map(buffer_out,&map,GST_MAP_WRITE);
            memcpy(map.data,packet_array->data,packet_array->len);
            g_array_unref(packet_array);
            gst_buffer_unmap(buffer_out,&map);
            return buffer_out;

        }
    }
    return NULL;
}

