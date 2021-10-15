
#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "ait.h"
#include "scte35.h"

GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser

gboolean packetize_ait(GstMpegtsAIT *ait,guint8 **pdata, guint *length )
{
   
    guint8 *ptr,*application_descriptors_loop_length;
    guint application_loop_length,common_descriptor_length,i,j;
    GstMpegtsDescriptor *descriptor;
    guint8* data;
    /* Smallest AIT section is the empty:
    * 10 bytes for the header
    * 0 bytes for the common descriptors
    * 2 bytes for the application loop length
    * 4 bytes for the CRC */
    *length = 16;

    /* Calculate size of common descriptors */
    common_descriptor_length = 0;
    for (i = 0; i < ait->common_descriptors->len; i++) {
        descriptor = g_ptr_array_index (ait->common_descriptors, i);
        common_descriptor_length += descriptor->length + 2;
    }
    *length += common_descriptor_length;

    application_loop_length = 0;
    /* Add the size of applications */
    for (i = 0; i < ait->applications->len; i++) {
        GstMpegtsApplication *app = g_ptr_array_index (ait->applications, i);
        /* There are at least +9 bytes 
        * 4 bytes organization_id
        * 2 bytes application_id
        * 1 byte application_control_code
        * 2 bytes application_descriptor_loop_length */

        application_loop_length += 9;
        for (j = 0; j < app->descriptors->len; j++) {
            descriptor = g_ptr_array_index (app->descriptors, j);
            application_loop_length += descriptor->length + 2;
        }
    }
    *length += application_loop_length;

    // Consrtuct the actual section representation
    *pdata=g_new0 (guint8,*length);
    data=*pdata;
    data[0]=0x74;   // fixed table ID for AIT section
//    data[1]=0x80 | (((*length-3)&0xff00)>>8); //section_syntax | section_length high byte
//    data[2]=(*length-3)&0xff; //section_length low byte
    GST_WRITE_UINT16_BE(&data[1],(*length-3)|(0xf<<12));
    GST_WRITE_UINT16_BE(&data[3],ait->application_type&0x7fff); // keep test_application_flag=0 !!!!
    data[5] = (ait->version_number<<1)|0xc1; 
    data[6] = ait->section_number;
    data[7] = ait->last_section_number;
    GST_WRITE_UINT16_BE(&data[8],common_descriptor_length|0xf000);
    /* Add the common descriptor array */
    ptr=&data[10];
    _packetize_descriptor_array (ait->common_descriptors, &ptr);
    GST_WRITE_UINT16_BE(ptr,application_loop_length|0xf000);
    ptr+=2;
    /* From here on we have the application loop */
    for (i = 0; i < ait->applications->len; i++) {
        GstMpegtsApplication *app = g_ptr_array_index (ait->applications, i);
        
        GST_WRITE_UINT32_BE(ptr,app->organization_id);
        ptr+=4;
        GST_WRITE_UINT16_BE(ptr,app->application_id);
        ptr+=2;
        *ptr=app->application_control_code;
        ptr+=1;
        application_descriptors_loop_length=ptr;
        ptr+=2;
        _packetize_descriptor_array(app->descriptors,&ptr);
        GST_WRITE_UINT16_BE(application_descriptors_loop_length,(ptr-application_descriptors_loop_length-2)|0xf<<12);      
    }
     /* CALCULATE AND WRITE THE CRC ! */
    GST_WRITE_UINT32_BE (ptr, _calc_crc32 (data, ptr - data));
    return TRUE;
}   
    
gboolean create_transport_protocol_descriptor(GstMpegtsDescriptor **pdesc,gchar* url_base)
{
    /* Currently implements only protocol_id=3: Transport via HTTP. See section 5.3.6 and 5.3.6.2
     * Descriptor length is:
     *  5 byte header
     *  len(url_base)+ 2 */
    guint8 *p,*s;
    guint8 descriptor_length;
    
    descriptor_length=strlen(url_base)+5; 
    p=g_new0(guint8,descriptor_length);
    GST_WRITE_UINT16_BE(&p[0],0x0003); //protocol_id: transport via HTTP
    p[2]=1; //transport_protocol_label
    p[3]=strlen(url_base);
    //From now on p points to the next byte after url_base
    s=(guint8*)g_stpcpy((gchar*)&p[4],url_base);
    *s=0;
    *pdesc=gst_mpegts_descriptor_from_custom(0x02,p,descriptor_length);
    return TRUE;
}

// Paragraph 5.3.5.3
gboolean create_application_descriptor(GstMpegtsDescriptor **pdesc)
{
    guint8 *p;
    guint8 descriptor_length;
    descriptor_length=9;
    p=g_new(guint8,descriptor_length);
    p[0]=5; //profile_lenth
    GST_WRITE_UINT16_BE(&p[1],0x0000);
    p[3]=1; //version.major
    p[4]=1; //version.minor
    p[5]=1; //version.micro
    p[6]=0xff; //service_bound_flag,visibility
    p[7]=1; //application_priority
    p[8]=1; //transport_protocol_label
    *pdesc=gst_mpegts_descriptor_from_custom(0x00,p,descriptor_length);
    return TRUE;
}

//Paragraph: 5.3.5.6.1
gboolean create_application_name_descriptor(GstMpegtsDescriptor **pdesc,gchar* app_name)
{
    guint8 *p;
    guint8 descriptor_length;
    descriptor_length=strlen(app_name)+4; // NULL at the end of app_name is not calculated
    p=g_new0(guint8,descriptor_length+1);
    p[0]='e';p[1]='n';p[2]='g';
    p[3]=strlen(app_name);
    strncpy((gchar*)&p[4],app_name,strlen(app_name));
    *pdesc=gst_mpegts_descriptor_from_custom(0x01,p,descriptor_length);
    return TRUE;
}


//Paragraph: 5.3.7
gboolean create_application_location_descriptor(GstMpegtsDescriptor **pdesc,gchar* initial_path)
{
    guint8 *p;
    guint8 descriptor_length;
    descriptor_length=strlen(initial_path);
    p=g_new0(guint8,descriptor_length+1);
    g_stpcpy((gchar*)&p[0],initial_path);
    if(strlen(initial_path)>0) {
      *pdesc=gst_mpegts_descriptor_from_custom(0x15,p,descriptor_length);
    } else {
      *pdesc=gst_mpegts_descriptor_from_custom(0x15,NULL,descriptor_length);
    }
    return TRUE;
}

//gboolean construct_ait(AIT **ait,gchar* url_base,gchar* initial_path,guint ait_pid,gchar* app_name)
gboolean construct_ait(AIT *ait)
{
    GstMpegtsAIT *tsait;
    GstMpegtsApplication *app;
    GstMpegtsDescriptor *p; 
    guint section_length;
    guint8 *raw_ait; 
    if(!ait->ait_defined) {
        GST_INFO("AIT is not defined.");
        return TRUE;
    }
    tsait=ait->mpegtsAIT=g_new0(GstMpegtsAIT,1);
    tsait->application_type=0x10; // HBBTV, registered @ dvbservices.com/identifiers/mhp_app_type_id
    tsait->test_application_flag=0;
    tsait->last_section_number=tsait->section_number=0;
    tsait->version_number=g_random_int_range(0,16);
    //initialize common descriptors
    tsait->common_descriptors=g_ptr_array_new();
    // create application
    tsait->applications=g_ptr_array_new();
    app=g_new0(GstMpegtsApplication,1);
    app->organization_id=999; //0x00000400; // Undefined 
    app->application_id=1;
    app->application_control_code=1; // Autostart
    app->descriptors=g_ptr_array_new();
    if(!create_transport_protocol_descriptor(&p,ait->url_base)){
        return FALSE;
    }
    g_ptr_array_add(app->descriptors,p);
    if(!create_application_descriptor(&p)) {
        return FALSE;
    }
    g_ptr_array_add(app->descriptors,p);
    if(!create_application_name_descriptor(&p,ait->app_name)) {
        return FALSE;
    }
    g_ptr_array_add(app->descriptors,p);
    if(!create_application_location_descriptor(&p,ait->initial_path)){
        return FALSE;
    }
    g_ptr_array_add(app->descriptors,p);
    // Append app to the AIT section
    g_ptr_array_add(tsait->applications,app);
    // Now let's build the transport packet
    memset(ait->ts_packet,0xff,188);
    ait->ts_packet[0]=0x47; 
    ait->ts_packet[1]=((ait->ait_pid&0x3f00)>>8)|0x40;
    ait->ts_packet[2]=ait->ait_pid&0xff;
    ait->ts_packet[3]=0x10|((ait->cc++)%16);
    ait->ts_packet[4]=0;  // the famous pointer_field.
    packetize_ait(tsait,(guint8**)&raw_ait,&section_length);        
    if(section_length>182) {
        GST_ERROR("AIT section is too long. Only <183 bytes are supported.");
        return FALSE;
    }
    memcpy(&ait->ts_packet[5],raw_ait,section_length);
    return TRUE;
}
