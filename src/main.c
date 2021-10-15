/* GStreamer
 *
 * appsink-src.c: example for using appsink and appsrc.
 *
 * Copyright (C) 2008 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include <string.h>

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "config.h"
#include "buffer.h"
#include "eit.h"


GST_DEBUG_CATEGORY (ts_parser);
#define GST_CAT_DEFAULT ts_parser



struct Settings *conf;



static void
advertise_service (GstElement * mux)
{
  GstMpegtsSDTService *service;
  GstMpegtsSDT *sdt;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *section;

  sdt = gst_mpegts_sdt_new ();

  sdt->actual_ts = TRUE;
  sdt->transport_stream_id = 1;
  sdt->original_network_id=0x3;

  service = gst_mpegts_sdt_service_new ();
  service->service_id = 1;
  service->running_status =
      GST_MPEGTS_RUNNING_STATUS_RUNNING ;
  service->EIT_schedule_flag = FALSE;
  service->EIT_present_following_flag = TRUE;
  service->free_CA_mode = FALSE;

  desc = gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, "hbbtv-test", "ljsoks2");

  g_ptr_array_add (service->descriptors, desc);
  g_ptr_array_add (sdt->services, service);

  section = gst_mpegts_section_from_sdt (sdt);
  gst_mpegts_section_send_event (section, mux);
  gst_mpegts_section_unref (section);
}

static void 
add_nit(GstElement *mux)
{
  GstMpegtsNIT *nit;
  GstMpegtsSection *section;
  GstMpegtsNITStream *stream;
  GstMpegtsDescriptor *desc,*network_name, *cable_delivery_desc;
  guint8 service_list[]={0x00,0x01,0x16};
  guint8 cable_parameters[]={0x06,0x00,0x00,0x00,0xff,0xf0,0x05,0x00,0x69,0x00,0x00};
  nit=gst_mpegts_nit_new();
  nit->actual_network=TRUE;
  nit->network_id=0x1;
  network_name=gst_mpegts_descriptor_from_dvb_network_name("ljsoks");
  cable_delivery_desc=gst_mpegts_descriptor_from_custom(0x44,cable_parameters,11);
  g_ptr_array_add(nit->descriptors,network_name);
  stream=gst_mpegts_nit_stream_new();
  stream->transport_stream_id=1;
  stream->original_network_id=0x3;

  desc=gst_mpegts_descriptor_from_custom(0x41,service_list,3);
  g_ptr_array_add(stream->descriptors,desc);
  g_ptr_array_add(stream->descriptors,cable_delivery_desc);
  g_ptr_array_add(nit->streams,stream);

  section=gst_mpegts_section_from_nit(nit);
  gst_mpegts_section_send_event(section,mux);
  gst_mpegts_section_unref(section);

}

static void
add_eit(GstElement *mux)
{
  GstMpegtsEIT *eit;
  GstMpegtsEITEvent *event;
  GstMpegtsDescriptor *short_event_desc;
  GstMpegtsSection *section;

  eit=gst_mpegts_eit_new( GST_MTS_TABLE_ID_EVENT_INFORMATION_ACTUAL_TS_PRESENT);
  event=gst_mpegts_eit_event_new();
  eit->transport_stream_id=1;
  eit->original_network_id=0x3;
  eit->segment_last_section_number=0;
  eit->last_table_id=0x4e;
  eit->actual_stream=TRUE;
  eit->present_following=TRUE;
  event->event_id=1;
  event->start_time=gst_date_time_new_now_utc();
  event->duration=0x033000;
  event->running_status=GST_MPEGTS_RUNNING_STATUS_RUNNING;
  event->free_CA_mode=FALSE;
  short_event_desc=create_short_event_descriptor("hun","test title","test description");
  g_ptr_array_add((GPtrArray*)event->descriptors,short_event_desc);
  g_ptr_array_add((GPtrArray*)eit->events,event);
  section=gst_mpegts_section_from_eit(eit,1);
  gst_mpegts_section_send_event(section,mux);
  gst_mpegts_section_unref(section);
}

/* called when the appsink notifies us that there is a new buffer ready for
 * processing */
static GstFlowReturn
on_new_sample_from_sink(GstElement* elt, ProgramData* data)
{
    GstSample *sample_in;
    GstSample *sample_out;
    GstBuffer *buffer_in;
    GstBuffer *buffer_out;
    GstElement* source;
    GstFlowReturn ret;

    /* get the sample from appsink */
    sample_in = gst_app_sink_pull_sample(GST_APP_SINK(elt));
    sample_out= gst_sample_copy(sample_in);
    buffer_in = gst_sample_get_buffer(sample_in);
    gst_buffer_ref(buffer_in);
    /* make a copy */
    /* app_buffer = gst_buffer_copy(buffer); */
    /* Process buffer */
    buffer_out=process_buffer(buffer_in,data);
    if(buffer_out) {
        /* we don't need the appsink sample anymore */
        /* get source an push new buffer */
        gst_sample_set_buffer(sample_out,buffer_out);
        gst_buffer_unref(buffer_in);
        gst_sample_unref(sample_in);
        source = gst_bin_get_by_name(GST_BIN(data->sink), "ts_src");
        ret = gst_app_src_push_sample(GST_APP_SRC(source), sample_out);
        gst_object_unref(source);
        gst_buffer_unref(buffer_out);
        gst_sample_unref(sample_out);
        return ret;
    } else {
        return GST_FLOW_ERROR;
    }
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we
 * notify the appsrc of it. */
static gboolean
on_source_message(GstBus* bus, GstMessage* message, ProgramData* data)
{
    GstElement* source;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        GST_INFO("The source got dry.");
        source = gst_bin_get_by_name(GST_BIN(data->sink), "ts_src");
        gst_app_src_end_of_stream(GST_APP_SRC(source));
        gst_object_unref(source);
        break;
    case GST_MESSAGE_ERROR:
        GST_ERROR("Received error from source.");
        g_main_loop_quit(data->loop);
        break;
    default:
        break;
    }
    return TRUE;
}

/* called when we get a GstMessage from the sink pipeline when we get EOS, we
 * exit the mainloop and this testapp. */
static gboolean
on_sink_message(GstBus* bus, GstMessage* message, ProgramData* data)
{
    /* nil */
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        GST_INFO("Finished playback.");
        g_main_loop_quit(data->loop);
        break;
    case GST_MESSAGE_ERROR:
        GST_ERROR("Received error from sink.");
        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data->sink),GST_DEBUG_GRAPH_SHOW_ALL,"sink");
        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data->source),GST_DEBUG_GRAPH_SHOW_ALL,"source");
        g_main_loop_quit(data->loop);
        break;
    default:
        break;
    }
    return TRUE;
}

int
main(int argc, char* argv[])
{
    GKeyFile *keyfile = NULL;
    GError *error = NULL;
    ProgramData* data = NULL;
    GstBus* bus = NULL;
    GstElement* ts_sink = NULL;
    GstElement* ts_src = NULL;
		GstElement *mux=NULL;

    gst_init(&argc, &argv);
    GST_DEBUG_CATEGORY_INIT(ts_parser,"ts_parser",0,"Here we are...");
    gst_mpegts_initialize();
    data = g_new0(ProgramData, 1);
    data->txt = new_txt_buffer();
    data->scte35=g_new0(SCTE35,1);
    data->ait=g_new0(AIT,1);
    data->pmt=g_new0(PMT,1);
    data->tdt=g_new0(TDT,1);
    data->tot=g_new0(TOT,1);
    if (argc == 2) {
        if(g_file_test(argv[1],G_FILE_TEST_EXISTS)) {
            keyfile=g_key_file_new();
            if(g_key_file_load_from_file(keyfile,argv[1],G_KEY_FILE_NONE,&error)) {
                conf=g_slice_new(Settings);
                conf->src_pipeline=g_key_file_get_string(keyfile,"pipeline","src_pipeline",NULL);
                conf->sink_pipeline=g_key_file_get_string(keyfile,"pipeline","sink_pipeline",NULL);
                conf->pmt_pid=g_key_file_get_integer(keyfile,"pmt","pmt_pid",NULL);
                conf->has_scte35=g_key_file_has_group(keyfile,"scte35");
                conf->txt_pid=g_key_file_get_integer(keyfile,"scte35","txt_pid",NULL);
                data->pmt->pmt_defined=g_key_file_has_group(keyfile,"pmt");
                data->scte35->scte35_defined=conf->has_scte35;
                data->scte35->txt_pid=conf->txt_pid;
                data->scte35->scte35_pid=g_key_file_get_integer(keyfile,"scte35","scte35_pid",NULL);
                data->txt->splice_in=g_key_file_get_integer(keyfile,"scte35","splice_in",NULL);
                data->txt->splice_out=g_key_file_get_integer(keyfile,"scte35","splice_out",NULL);
                data->txt->preroll=g_key_file_get_integer(keyfile,"scte35","preroll",NULL);         
                data->ait->ait_defined=g_key_file_has_group(keyfile,"ait");
                data->pmt->pmt_pid=conf->pmt_pid;
                data->tdt->tdt_defined=g_key_file_has_group(keyfile,"tdt");
                data->tot->tot_defined=g_key_file_has_group(keyfile,"tot");
                if(data->ait->ait_defined) {
                  data->ait->ait_pid=g_key_file_get_integer(keyfile,"ait","ait_pid",NULL);
                  data->ait->url_base=g_key_file_get_string(keyfile,"ait","url_base",NULL);
                  data->ait->initial_path=g_key_file_get_string(keyfile,"ait","initial_path",NULL);
                  data->ait->ait_stream=g_key_file_get_string(keyfile,"ait","ait_stream",NULL);
                  data->ait->app_name=g_key_file_get_string(keyfile,"ait","app_name",NULL);
                  if(!construct_ait(data->ait)){
                    GST_ERROR("Couldn't build AIT section.");
                    return -1;
                  }
                }
                if(data->tdt->tdt_defined) {
                  if(!construct_tdt(data->tdt)) {
                    GST_ERROR("Couldn't build TDT section.");
                    return -1;
                  }
                } 
                if(data->tot->tot_defined) {
                  data->tot->country_code=g_key_file_get_string(keyfile,"tot","country_code",NULL);
                  data->tot->country_region_id=g_key_file_get_integer(keyfile,"tot","country_region_id",NULL);
                  data->tot->local_time_offset_polarity=g_key_file_get_integer(keyfile,"tot","local_time_offset_polarity",NULL);
                  data->tot->local_time_offset=g_key_file_get_integer(keyfile,"tot","local_time_offset",NULL);
                  data->tot->time_of_change=g_key_file_get_string(keyfile,"tot","time_of_change",NULL);
                  data->tot->next_time_offset=g_key_file_get_integer(keyfile,"tot","next_time_offset",NULL);
                  if(!construct_tot(data->tot)) {
                    GST_ERROR("Couldn't build TOT section.");
                    return -1;
                  } 
                }
            } else {
                GST_ERROR("Couldn't parse config file.");
                GST_ERROR(error->message);
                return -1;
            }
        } else {
            GST_ERROR("Cannot find config file.");
            return -1;
        }            
    }
    else {
        GST_ERROR("Missing config file argument. Usage: x31_to_scte35 <config_file>");
        return -1;
    }
    data->loop = g_main_loop_new(NULL, FALSE);

    /* setting up source pipeline, we read from a file and convert to our desired
     * caps. */
    data->source = gst_parse_launch(conf->src_pipeline, NULL);

    if (data->source == NULL) {
        GST_ERROR("Bad source.");
        g_main_loop_unref(data->loop);
        g_free(data);
        return -1;
    }

		// This code will insert standard DVB sections. Works only if we have the mpegtsmux element in the pipeline.
		mux = gst_bin_get_by_name (GST_BIN (data->source), "mux");
		if(mux!=NULL) {
	  	advertise_service (mux);
      add_nit(mux);
      add_eit(mux);
  		gst_object_unref (mux);
		}

    /* to be notified of messages from this pipeline, mostly EOS */
    bus = gst_element_get_bus(data->source);
    gst_bus_add_watch(bus, (GstBusFunc)on_source_message, data);
    gst_object_unref(bus);

    /* we use appsink in push mode, it sends us a signal when data is available
     * and we pull out the data in the signal callback. We want the appsink to
     * push as fast as it can, hence the sync=false */
    ts_sink = gst_bin_get_by_name(GST_BIN(data->source), "ts_sink");
    g_object_set(G_OBJECT(ts_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(ts_sink, "new-sample",
        G_CALLBACK(on_new_sample_from_sink), data);
    gst_object_unref(ts_sink);

    /* setting up sink pipeline, we push audio data into this pipeline that will
     * then play it back using the default audio sink. We have no blocking
     * behaviour on the src which means that we will push the entire file into
     * memory. */
    data->sink = gst_parse_launch(conf->sink_pipeline, NULL);

    if (data->sink == NULL) {
        GST_ERROR("Bad sink.");
        gst_object_unref(data->source);
        g_main_loop_unref(data->loop);
        g_free(data);
        return -1;
    }

    ts_src = gst_bin_get_by_name(GST_BIN(data->sink), "ts_src");
    /* configure for time-based format */
    g_object_set(ts_src, "format", GST_FORMAT_TIME, NULL);
    /* uncomment the next line to block when appsrc has buffered enough */
    /* g_object_set (ts_src, "block", TRUE, NULL); */
    gst_object_unref(ts_src);

    bus = gst_element_get_bus(data->sink);
    gst_bus_add_watch(bus, (GstBusFunc)on_sink_message, data);
    gst_object_unref(bus);

    /* launching things */
    gst_element_set_state(data->sink, GST_STATE_PLAYING);
    gst_element_set_state(data->source, GST_STATE_PLAYING);

    /* let's run !, this loop will quit when the sink pipeline goes EOS or when an
     * error occurs in the source or sink pipelines. */
    GST_INFO("Let's run!");
    g_main_loop_run(data->loop);
    GST_INFO("Going out.");

    gst_element_set_state(data->source, GST_STATE_NULL);
    gst_element_set_state(data->sink, GST_STATE_NULL);

    gst_object_unref(data->source);
    gst_object_unref(data->sink);
    g_main_loop_unref(data->loop);
    g_free(data);

    return 0;
}

