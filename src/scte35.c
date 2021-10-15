
#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "scte35.h"
#include "config.h"

extern Settings *conf;

GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser


static const guint32 crc_tab[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};


guint32
_calc_crc32 (const guint8 * data, guint datalen)
{
  gint i;
  guint32 crc = 0xffffffff;

  for (i = 0; i < datalen; i++) {
    crc = (crc << 8) ^ crc_tab[((crc >> 24) ^ *data++) & 0xff];
  }
  return crc;
}


void
_packetize_descriptor_array (GPtrArray * array, guint8 ** out_data)
{
  guint i;
  GstMpegtsDescriptor *descriptor;

  g_return_if_fail (out_data != NULL);
  g_return_if_fail (*out_data != NULL);

  if (array == NULL)
    return;

  for (i = 0; i < array->len; i++) {
    descriptor = g_ptr_array_index (array, i);

    memcpy (*out_data, descriptor->data, descriptor->length + 2);
    *out_data += descriptor->length + 2;
  }
}


gboolean packetize_sit(GstMpegtsSCTESIT *sit,guint8 **pdata, guint *length )
{
   
    guint8 *ptr;
    guint command_length,descriptor_length,i;
    GstMpegtsDescriptor *descriptor;
    guint32 tmp32;
    guint8* data;
    /* Smallest splice section are the NULL and bandwith command:
       * 14 bytes for the header
    * 0 bytes for the command
    * 2 bytes for the empty descriptor loop length
    * 4 bytes for the CRC */
    *length = 20;

    command_length = 0;
    /* Add the size of splices */
    for (i = 0; i < sit->splices->len; i++) {
        GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);
        /* There is at least 5 bytes */
        command_length += 5;
        if (!event->splice_event_cancel_indicator) {
            if (!event->program_splice_flag) {
                GST_ERROR ("Only SCTE program splices are supported");
                return FALSE;
            }
            /* Add at least 5 bytes for common fields */
            command_length += 5;
            if (!event->splice_immediate_flag) {
                if (event->program_splice_time_specified)
                    command_length += 5;
                else
                    command_length += 1;
            }
            if (event->duration_flag)
                command_length += 5;
        }
    }
    *length += command_length;

    /* Calculate size of descriptors */
    descriptor_length = 0;
    for (i = 0; i < sit->descriptors->len; i++) {
        descriptor = g_ptr_array_index (sit->descriptors, i);
        descriptor_length += descriptor->length + 2;
    }
    *length += descriptor_length;
    data=*pdata=g_malloc (*length);
    data[0]=0xfc;   // fixed table ID for splice_info_section
    data[1]=((*length-3)&0xff00)>>8; //section_syntax, private_indicator, section_length high byte
    data[2]=(*length-3)&0xff; //section_length low byte
    data[3] = 0; // protocol version
    /* 7bits for encryption (not supported : 0) */
    /* 33bits for pts_adjustment */
    data[4] = (sit->pts_adjustment) >> 32 & 0x01;
    GST_WRITE_UINT32_BE (&data[5], sit->pts_adjustment & 0xffffffff);
    /* cw_index : 8 bit */
    data[9] = sit->cw_index;
    /* tier                  : 12bit
    * splice_command_length : 12bit
    * splice_command_type   : 8 bit */
    tmp32 = (sit->tier & 0xfff) << 20;
    tmp32 |= (command_length & 0xfff) << 8;
    tmp32 |= sit->splice_command_type;
    GST_WRITE_UINT32_BE (&data[10], tmp32);
    ptr=&data[14];
    /* Write the events */
    for (i = 0; i < sit->splices->len; i++) {
        GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);

        /* splice_event_id : 32bit */
        GST_WRITE_UINT32_BE (ptr, event->splice_event_id);
        ptr += 4;
        /* splice_event_cancel_indicator : 1bit
        * reserved                      ; 7bit */
        *ptr++ = event->splice_event_cancel_indicator ? 0xff : 0x7f;
        if (!event->splice_event_cancel_indicator) {
            /* out_of_network_indicator : 1bit
            * program_splice_flag      : 1bit
            * duration_flag            : 1bit
            * splice_immediate_flag    : 1bit
            * reserved                 : 4bit */
            *ptr++ = (event->out_of_network_indicator << 7) |
                (event->program_splice_flag << 6) |
                (event->duration_flag << 5) |
                (event->splice_immediate_flag << 4) | 0x0f;
            if (!event->splice_immediate_flag) {
                /* program_splice_time_specified : 1bit
                * reserved : 6/7 bit */
                if (!event->program_splice_time_specified)
                    *ptr++ = 0x7f;
                else {
                    /* time : 33bit */
                    *ptr++ = 0xf2 | ((event->program_splice_time >> 32) & 0x1);
                    GST_WRITE_UINT32_BE (ptr, event->program_splice_time & 0xffffffff);
                    ptr += 4;
                }
            }
            if (event->duration_flag) {
                *ptr = event->break_duration_auto_return ? 0xfe : 0x7e;
                *ptr++ |= (event->break_duration >> 32) & 0x1;
                GST_WRITE_UINT32_BE (ptr, event->break_duration & 0xffffffff);
                ptr += 4;
            }
            /* unique_program_id : 16bit */
            GST_WRITE_UINT16_BE (ptr, event->unique_program_id);
            ptr += 2;
            /* avail_num : 8bit */
            *ptr++ = event->avail_num;
            /* avails_expected : 8bit */
            *ptr++ = event->avails_expected;
        }   
    }
    /* Descriptors */
    GST_WRITE_UINT16_BE (ptr, descriptor_length);
    ptr += 2;
    _packetize_descriptor_array (sit->descriptors, &ptr);

    /* CALCULATE AND WRITE THE CRC ! */
    GST_WRITE_UINT32_BE (ptr, _calc_crc32 (data, ptr - data));
    return TRUE;
}

static void
_gst_mpegts_scte_splice_event_free (GstMpegtsSCTESpliceEvent * event)
{
  g_slice_free (GstMpegtsSCTESpliceEvent, event);
}


/**
 * gst_mpegts_scte_sit_new:
 *
 * Allocates and initializes a #GstMpegtsSCTESIT.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_sit_new (void)
{
  GstMpegtsSCTESIT *sit;

  sit = g_slice_new0 (GstMpegtsSCTESIT);

  /* Set all default values (which aren't already 0/NULL) */
  sit->tier = 0xfff;

  sit->splices = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_scte_splice_event_free);
  sit->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  return sit;
}

/**
 * gst_mpegts_scte_splice_event_new:
 *
 * Allocates and initializes a #GstMpegtsSCTESpliceEvent.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESpliceEvent
 */
GstMpegtsSCTESpliceEvent *
gst_mpegts_scte_splice_event_new (void)
{
  GstMpegtsSCTESpliceEvent *event = g_slice_new0 (GstMpegtsSCTESpliceEvent);

  /* Non-0 Default values */
  event->program_splice_flag = TRUE;

  return event;
}


GstMpegtsSCTESIT *
gst_mpegts_scte_null_new (void)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_NULL;
  return sit;
}

/**
 * gst_mpegts_scte_splice_in_new:
 * @event_id: The event ID.
 * @splice_time: The PCR time for the splice event
 *
 * Allocates and initializes a new "Splice In" INSERT command
 * #GstMpegtsSCTESIT for the given @event_id and @splice_time.
 *
 * If the @splice_time is #G_MAXUINT64 then the event will be
 * immediate as opposed to for the target @splice_time.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_splice_in_new (guint32 event_id, guint64 splice_time)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();
  GstMpegtsSCTESpliceEvent *event = gst_mpegts_scte_splice_event_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;
  event->splice_event_id = event_id;
  if (splice_time == G_MAXUINT64) {
    event->splice_immediate_flag = TRUE;
  } else {
    event->program_splice_time_specified = TRUE;
    event->program_splice_time = splice_time;
  }
  g_ptr_array_add (sit->splices, event);

  return sit;
}

/**
 * gst_mpegts_scte_splice_out_new:
 * @event_id: The event ID.
 * @splice_time: The PCR time for the splice event
 * @duration: The optional duration.
 *
 * Allocates and initializes a new "Splice Out" INSERT command
 * #GstMpegtsSCTESIT for the given @event_id, @splice_time and
 * duration.
 *
 * If the @splice_time is #G_MAXUINT64 then the event will be
 * immediate as opposed to for the target @splice_time.
 *
 * If the @duration is 0 it won't be specified in the event.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_splice_out_new (guint32 event_id, guint64 splice_time,
    guint64 duration)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();
  GstMpegtsSCTESpliceEvent *event = gst_mpegts_scte_splice_event_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;
  event->splice_event_id = event_id;
  event->out_of_network_indicator = TRUE;
  if (splice_time == G_MAXUINT64) {
    event->splice_immediate_flag = TRUE;
  } else {
    event->program_splice_time_specified = TRUE;
    event->program_splice_time = splice_time;
  }
  if (duration != 0) {
    event->duration_flag = TRUE;
    event->break_duration = duration;
  }
  g_ptr_array_add (sit->splices, event);

  return sit;
}



gboolean add_splice(guint8 type, GArray* packet_array,SCTE35* scte35,guint32 event_id,guint64 splice_time)
{
    guint8 ts_packet[188];
    guint8 *data;
    guint section_length;
    GstMpegtsSCTESIT *sit=NULL;
    // This is the ts packet header
    memset(ts_packet,0xff,188);
    ts_packet[0]=0x47; 
    ts_packet[1]=((scte35->scte35_pid&0x3f00)>>8)|0x40;
    ts_packet[2]=scte35->scte35_pid&0xff;
    ts_packet[3]=0x10|((scte35->cc++)%16);
    ts_packet[4]=0;  // the famous pointer_field.
    // From now on this is the splice_info_section
    switch(type) {
        case 'N':
            sit=gst_mpegts_scte_null_new();
            break;
        case 'I':
            sit=gst_mpegts_scte_splice_in_new(event_id,splice_time);
            GST_INFO("Inserted splice_in event with eventid: %u  PTS: %"GST_TIME_FORMAT"", event_id,GST_TIME_ARGS(MPEGTIME_TO_GSTTIME(splice_time)));
            break;
        case 'O':
            sit=gst_mpegts_scte_splice_out_new(event_id,splice_time,0);
            GST_INFO("Inserted splice_out event with eventid: %u  PTS: %"GST_TIME_FORMAT"", event_id,GST_TIME_ARGS(MPEGTIME_TO_GSTTIME(splice_time)));
            break;
        default:
            GST_ERROR("Unknown splice request. Only NULL, IN and OUT are supported.");
            return FALSE;
    }
    if(packetize_sit(sit,&data,&section_length)) {
        if(section_length<184) {
            memcpy(&ts_packet[5],data,section_length);
            g_array_append_vals(packet_array,ts_packet,188);
            g_free(data);
            //gst_mpegts_section_unref(sit);
//            g_free(sit);
            g_print("n\r");
    
            return TRUE;
        } else {
            GST_ERROR(" SCTE35 section is too long. Not supported as of now. ");
            return FALSE;
        }
    }
    GST_ERROR("Couldn't packetize splice_null.");
    return FALSE;
}

