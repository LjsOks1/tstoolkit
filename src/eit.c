
#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "eit.h"
#include "tdt.h"
#include "scte35.h"

GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser


void gst_mpegts_eit_event_free(GstMpegtsEITEvent *event)
{
	if(event->descriptors) {
		g_ptr_array_unref(event->descriptors);
	}
	g_slice_free(GstMpegtsEITEvent,event);
}

GstMpegtsEITEvent* gst_mpegts_eit_event_new()
{
	GstMpegtsEITEvent* event;
	event=g_slice_new0(GstMpegtsEITEvent);
	event->descriptors=g_ptr_array_new_with_free_func((GDestroyNotify)gst_mpegts_descriptor_free);
	return event;
}

GstMpegtsEITEvent* gst_mpegts_eit_event_copy (GstMpegtsEITEvent *event)
{
  GstMpegtsEITEvent *copy = g_slice_dup (GstMpegtsEITEvent, event);

  copy->descriptors = g_ptr_array_ref (event->descriptors);

  return copy;
}

G_DEFINE_BOXED_TYPE (GstMpegtsEITEvent, gst_mpegts_eit_event,
    (GBoxedCopyFunc) gst_mpegts_eit_event_copy,
    (GFreeFunc) gst_mpegts_eit_event_free);


void gst_mpegts_eit_free (GstMpegtsEIT * eit)
{
  g_ptr_array_unref (eit->events);
  g_slice_free (GstMpegtsEIT, eit);
}

GstMpegtsEIT* gst_mpegts_eit_new(guint8 table_id)
{

	GstMpegtsEIT *eit;

  eit = g_slice_new0 (GstMpegtsEIT);

  eit->events = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_eit_event_free);

  return eit;
}

GstMpegtsEIT * gst_mpegts_eit_copy (GstMpegtsEIT * eit)
{
  GstMpegtsEIT *copy = g_slice_dup (GstMpegtsEIT, eit);

  copy->events = g_ptr_array_ref (eit->events);

  return copy;
}


G_DEFINE_BOXED_TYPE (GstMpegtsEIT, gst_mpegts_eit,
    (GBoxedCopyFunc) gst_mpegts_eit_copy, (GFreeFunc) gst_mpegts_eit_free);


void get_event_descriptor_size(gpointer p,gpointer s)
{
	GstMpegtsDescriptor *desc=(GstMpegtsDescriptor*)p;
	guint* size=(guint*)s;
	(*size)+=desc->length+2;
}

void get_event_size(gpointer p, gpointer s)
{
	GstMpegtsEITEvent *event=(GstMpegtsEITEvent*)p;
	guint* size=(guint*)s;

	(*size)+=12;  // Header size of events without descriptors

	g_ptr_array_foreach((GPtrArray*)event->descriptors,get_event_descriptor_size,size);
}

/* Moves p pointer ahead as packetizing descriptors.
 * p points to the next byte after the descriptor */
void parse_descriptors(gpointer d,gpointer gp)
{
  GstMpegtsDescriptor *desc=(GstMpegtsDescriptor*)d;
  guint8** p=(guint8**)gp;
//  *((*p)++) = desc->tag;
//  *((*p)++) = desc->length;
  memcpy(*p,desc->data,desc->length+2);
  *p+=desc->length+2;
}

GstMpegtsDescriptor* create_short_event_descriptor(char* lang_code,char* title, char* description)
{
  guint8 *p,*data;
  guint8 length;
  length=5+strlen(title)+strlen(description);
  GstMpegtsDescriptor *desc;

  p=g_new0(guint8,length);
  data=p;
  memcpy(p,lang_code,3);
  p+=3;
  *(p++)=strlen(title);
  memcpy(p,title,strlen(title));
  p+=strlen(title);
  *(p++)=strlen(description);
  memcpy(p,description,strlen(description));
  p+=strlen(description);
  desc=gst_mpegts_descriptor_from_custom(0x4d,data,length);
  return desc;
}


void parse_event(gpointer e,gpointer gp)
{
	GstMpegtsEITEvent* event=(GstMpegtsEITEvent*)e;
  guint8** p=(guint8**)gp;
  guint8 *desc_length;
  GST_WRITE_UINT16_BE(*p,event->event_id);   //event_id
  *p+=2;
  get_tdt_time(gst_date_time_to_g_date_time(event->start_time),*p);      //start_time
  *p+=5;
  GST_WRITE_UINT24_BE(*p,event->duration);  // duration must be BCD encoded in HHMMSS format; (3 bytes) 
  *p+=3;
  desc_length=*p;
  *p+=2;
  g_ptr_array_foreach(event->descriptors,parse_descriptors,(gpointer)p);
  GST_WRITE_UINT16_BE(desc_length,((*p-desc_length-2)| ((event->running_status & 0x07) << 13) | event->free_CA_mode<<12));
}


gboolean _packetize_eit(GstMpegtsSection* eit_sec)
{
  if(eit_sec->data!=NULL)
    return TRUE;
  else
    return FALSE;
}
//FIXME: Only works for single section EIT. (Probably only present/following and not for schedule
GstMpegtsSection* gst_mpegts_section_from_eit (GstMpegtsEIT * eit,guint16 service_id)
{
  GstMpegtsSection *section;
	guint8 *data,*event_ptr; 
	guint data_size;

	/*calculate the size of the  data array */
	data_size=18;  //Size of the header bytes plus CRC at the end without the event loop
	g_ptr_array_foreach(eit->events,get_event_size,&data_size); 
	
	data=g_new0(guint8,data_size);

	/* build the data array */
	//FIXME: Table ID for event schedule information is more complicated....
	if(eit->actual_stream)
		if(eit->present_following)
			data[0]=0x4e;
		else
			data[0]=0x50;
	else
		if(eit->present_following)
			data[0]=0x4f;
		else
			data[0]=0x60;
	
	GST_WRITE_UINT16_BE(&data[1],(data_size-3)|0xf000); //section_length
	GST_WRITE_UINT16_BE(&data[3],service_id); // service_id
	data[5]=0xc1; // version_number=0; current_next_indicator=1;
	data[6]=0;		// section_number
	data[7]=0;	  // last_section_number
	GST_WRITE_UINT16_BE(&data[8],eit->transport_stream_id);  //transport stream id
	GST_WRITE_UINT16_BE(&data[10],eit->original_network_id);  //original network id
	data[12]=eit->segment_last_section_number;  //segment last section number
	data[13]=eit->last_table_id;  //last table id
  /* Event loop. event_ptr should point to the first byte of the CRC once function returns. */
  event_ptr=&data[14];
  g_ptr_array_foreach(eit->events,parse_event,&event_ptr);
  GST_WRITE_UINT32_BE(event_ptr,_calc_crc32(data,event_ptr-data)); 
	section=gst_mpegts_section_new(0x12,data,data_size);
  section->packetizer=_packetize_eit;
	return section;
}

