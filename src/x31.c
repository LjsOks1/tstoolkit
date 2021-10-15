#include <gst/gst.h>
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#include "config.h"
#include "x31.h"
#include "scte35.h"


extern Settings *conf;

GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser


static guint8 unham84tab[256] = {
    0x01, 0xff, 0x01, 0x01, 0xff, 0x00, 0x01, 0xff,
    0xff, 0x02, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x07,
    0xff, 0x00, 0x01, 0xff, 0x00, 0x80, 0xff, 0x00,
    0x06, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x03, 0xff,
    0xff, 0x0c, 0x01, 0xff, 0x04, 0xff, 0xff, 0x07,
    0x06, 0xff, 0xff, 0x07, 0xff, 0x07, 0x07, 0x87,
    0x06, 0xff, 0xff, 0x05, 0xff, 0x00, 0x0d, 0xff,
    0x86, 0x06, 0x06, 0xff, 0x06, 0xff, 0xff, 0x07,
    0xff, 0x02, 0x01, 0xff, 0x04, 0xff, 0xff, 0x09,
    0x02, 0x82, 0xff, 0x02, 0xff, 0x02, 0x03, 0xff,
    0x08, 0xff, 0xff, 0x05, 0xff, 0x00, 0x03, 0xff,
    0xff, 0x02, 0x03, 0xff, 0x03, 0xff, 0x83, 0x03,
    0x04, 0xff, 0xff, 0x05, 0x84, 0x04, 0x04, 0xff,
    0xff, 0x02, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x07,
    0xff, 0x05, 0x05, 0x85, 0x04, 0xff, 0xff, 0x05,
    0x06, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x03, 0xff,
    0xff, 0x0c, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x09,
    0x0a, 0xff, 0xff, 0x0b, 0x8a, 0x0a, 0x0a, 0xff,
    0x08, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x0d, 0xff,
    0xff, 0x0b, 0x0b, 0x8b, 0x0a, 0xff, 0xff, 0x0b,
    0x0c, 0x8c, 0xff, 0x0c, 0xff, 0x0c, 0x0d, 0xff,
    0xff, 0x0c, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x07,
    0xff, 0x0c, 0x0d, 0xff, 0x0d, 0xff, 0x8d, 0x0d,
    0x06, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x0d, 0xff,
    0x08, 0xff, 0xff, 0x09, 0xff, 0x09, 0x09, 0x89,
    0xff, 0x02, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x09,
    0x88, 0x08, 0x08, 0xff, 0x08, 0xff, 0xff, 0x09,
    0x08, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x03, 0xff,
    0xff, 0x0c, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x09,
    0x0f, 0xff, 0x8f, 0x0f, 0xff, 0x0e, 0x0f, 0xff,
    0x08, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x0d, 0xff,
    0xff, 0x0e, 0x0f, 0xff, 0x0e, 0x8e, 0xff, 0x0e,
};



/*  Useful standards to read: iso13818-1 for the TS paclket and eélementary streams
*                             EN 300 472 & EN 301 775 for teletext related things
*/



TXT* new_txt_buffer()
{
    TXT* txt=NULL;
    txt=g_new0(TXT,1);
    txt->pes_buffer=NULL;
    txt->PTS=0;
    txt->PES_packet_length=0;
    txt->cc=0;
    return txt;
}

gboolean clear_txt_buffer(TXT *txt)
{
    if(txt->pes_buffer!=NULL) {
        g_array_unref(txt->pes_buffer);
        txt->pes_buffer=NULL;
        txt->PTS=0;
        txt->PES_packet_length=0;
        txt->cc=0;
        return TRUE;
    }
    GST_ERROR("Shouldn't call clear_txt_buffer. Buffer is NULL.");
    return FALSE;
}

gboolean process_txt_pes_buffer( GArray* packet_array,TXT *txt,SCTE35 *scte35) 
{
    guint PES_packet_length,PES_header_data_length,PTS_DTS_flags;
    guint64 PTS =0;
    guint8* txt_payload;
    gsize pes_size;
    static guint64 last_scte_sent;
    guint64 current_time;
    
    current_time=g_get_real_time();

    guint8* ptr=(guint8*)g_array_steal(txt->pes_buffer,&pes_size);
    if((ptr[0]==0x00) && (ptr[1]==0x00) && (ptr[2]==0x01) && (ptr[3]==0xbd)){
        // starting with a private stream PES header: 0x00 0x00 0x01 0xBD
        PES_packet_length=(ptr[4]<<8) + ptr[5];
        PES_header_data_length=ptr[8];
        PTS_DTS_flags= (ptr[7] & 0xc0) >> 6;
        txt_payload=ptr+9+PES_header_data_length; 
        if(PTS_DTS_flags==2) {
            PTS=(((guint64)ptr[9] & 0x0e)<<29) + (ptr[10] << 22) + ((ptr[11] &0xfe) << 14) + (ptr[12] << 7) + ((ptr[13] & 0xfe) >>1);
        } else {
            return FALSE;
        }
        if(*txt_payload++>=0x10)  { //data_identifier: EBU data EN 300 472 (teletext)/EN 301 775; next byte:data_unit_id
            while (txt_payload < ptr+PES_packet_length+6) {
                if (*txt_payload == 0x02 || *txt_payload == 0x03) { //data_unit_id: EBU Teletext non-subtitle data or subtitle data
                    guint8 c1, c2, c;
                    guint8 packet_nr;
                    static guint gpis;
                    static guint event_id;
                    c1 = unham84tab[*(txt_payload+4)];
                    c2 = unham84tab[*(txt_payload+5)];
                    c = (c2 << 4) | (c1 & 0x0f);
                    packet_nr = (c >> 3) & 0x1f;
                    if (packet_nr == 31) { //We have an X31 packet!!!!
                        g_print("X\r");
                        if (*(txt_payload+11) != gpis) {
                            GST_INFO("NEW GPI values detected:0x%x. Old value was:0x%x",txt_payload[11],gpis);
                            if(txt_payload[11]== txt->splice_in) {
                                add_splice('I',packet_array,scte35,event_id,PTS+txt->preroll);
                                last_scte_sent=current_time;
                            } else if(txt_payload[11]==txt->splice_out) {
                                event_id=g_rand_int(g_rand_new());                            
                                add_splice('O',packet_array,scte35,event_id,PTS+txt->preroll);
                                last_scte_sent=current_time;
                            } else {
                                GST_INFO("No action associated to this GPI value (0x%x)",txt_payload[11]);
                            } 
                            gpis = *(txt_payload+11);
                        } else {
                            if(current_time>last_scte_sent+40*1000000) {
                              GST_LOG("Sending splice_null event.");
                              if(!add_splice('N',packet_array,scte35,0,0)) {
                                  return FALSE;
                              }
                              last_scte_sent=current_time;
                            }
                        }
                    }
                } 
                txt_payload += *(txt_payload+1)+1; //Move to the next txt data block in the TS packet payload
            }
        }
    } else {
        GST_ERROR("Couldn't decode PES header in teletext packet. X31 cannot be detected.");
        return FALSE; 
    }
    g_free(ptr);
    return TRUE;      
}

/* Old code, generate a lot og SCTE35 NULL packets. to be deleted later.
gboolean process_txt_pes_buffer( GArray* packet_array,TXT *txt,SCTE35 *scte35) 
{
    guint PES_packet_length,PES_header_data_length,PTS_DTS_flags;
    guint64 PTS =0;
    guint8* txt_payload;
    gsize pes_size;
    guint8* ptr=(guint8*)g_array_steal(txt->pes_buffer,&pes_size);
    if((ptr[0]==0x00) && (ptr[1]==0x00) && (ptr[2]==0x01) && (ptr[3]==0xbd)){
        // starting with a private stream PES header: 0x00 0x00 0x01 0xBD
        PES_packet_length=(ptr[4]<<8) + ptr[5];
        PES_header_data_length=ptr[8];
        PTS_DTS_flags= (ptr[7] & 0xc0) >> 6;
        txt_payload=ptr+9+PES_header_data_length; 
        if(PTS_DTS_flags==2) {
            PTS=(((guint64)ptr[9] & 0x0e)<<29) + (ptr[10] << 22) + ((ptr[11] &0xfe) << 14) + (ptr[12] << 7) + ((ptr[13] & 0xfe) >>1);
        } else {
            return FALSE;
        }
        if(*txt_payload++>=0x10)  { //data_identifier: EBU data EN 300 472 (teletext)/EN 301 775; next byte:data_unit_id
            while (txt_payload < ptr+PES_packet_length+6) {
                if (*txt_payload == 0x02 || *txt_payload == 0x03) { //data_unit_id: EBU Teletext non-subtitle data or subtitle data
                    guint8 c1, c2, c;
                    guint8 packet_nr;
                    static guint gpis;
                    static guint event_id;
                    c1 = unham84tab[*(txt_payload+4)];
                    c2 = unham84tab[*(txt_payload+5)];
                    c = (c2 << 4) | (c1 & 0x0f);
                    packet_nr = (c >> 3) & 0x1f;
                    if (packet_nr == 31) { //We have an X31 packet!!!!
                        g_print("X\r");
                        if (*(txt_payload+11) != gpis) {
                            GST_INFO("NEW GPI values detected:0x%x. Old value was:0x%x",txt_payload[11],gpis);
                            if(txt_payload[11]== txt->splice_in) {
                                add_splice('I',packet_array,scte35,event_id,PTS+txt->preroll);
                            } else if(txt_payload[11]==txt->splice_out) {
                                event_id=g_rand_int(g_rand_new());                            
                                add_splice('O',packet_array,scte35,event_id,PTS+txt->preroll);
                            } else {
                                GST_INFO("No action associated to this GPI value (0x%x)",txt_payload[11]);
                            } 
                            gpis = *(txt_payload+11);
                        } else {
                            GST_LOG("Sending splice_null event.");
                            if(!add_splice('N',packet_array,scte35,0,0)) {
                                return FALSE;
                            }
                        }
                    }
                } 
                txt_payload += *(txt_payload+1)+1; //Move to the next txt data block in the TS packet payload
            }
        }
    } else {
        GST_ERROR("Couldn't decode PES header in teletext packet. X31 cannot be detected.");
        return FALSE; 
    }
    g_free(ptr);
    return TRUE;      
}
*/

/* Builds the PES packet from the ts packets.
*  If the complete PES packet is available process_x31() is called to find the relevant X31 packets.
*/
gboolean push_txt_packet(guint8 *ptr, GArray *packet_array, TXT *txt,SCTE35* scte35)
{
//FIXME: Can we have adaptation field as stuffing bytes?????
    if( ((ptr[1] & 0x80) == 0) &&       // Transport error indicator == 0
        ((ptr[3] & 0xc0) == 0) ) {       // Scrambling control == 0
        
        if( ((ptr[3] & 0x30) == 0x10)) {    // No adaptation field, payload only
            if((ptr[1] & 0x40) !=0 ) {      // If PUSI==1 process the existing PES and start a new new one.
                if(txt->pes_buffer!=NULL) { // We have data in txt_pes_buffer, let's process it first, than clear it.
                    process_txt_pes_buffer(packet_array,txt,scte35);
                    clear_txt_buffer(txt);
                }
                // Let's start a new pes_buffer
                txt->pes_buffer=g_array_new(FALSE,TRUE,1);
                txt->cc=ptr[3]&0x0f;
                // We have PUSI so let's check if we have the PES header
                if((ptr[4]==0x00) && (ptr[5]==0x00) && (ptr[6]==0x01) && (ptr[7]==0xbd)){
                    // starting with a private stream PES header: 0x00 0x00 0x01 0xBD
                    // Copy ts packet to the PES...
                    g_array_append_vals(txt->pes_buffer,&ptr[4],184);                               
                    return TRUE;
                } else {
                    GST_ERROR("PUSI ==1 but ts packet payload is not starting with a PES header.");
                    return FALSE;
                }          
            } else { // PUSI==0; check if CC is ok, and add the TS packet to the PES
                if(txt->pes_buffer!=NULL) { // only if pes_buffer already exists...
                    if(((++txt->cc)%16)==(ptr[3]&0x0f)) {
                        g_array_append_vals(txt->pes_buffer,&ptr[4],184);
                        return TRUE;
                    } else { // cc error, clear the PES buffer
                        GST_ERROR("CC error detected on TXT pid.");
                        clear_txt_buffer(txt);
                        return FALSE;
                    }
                } else { // PUSI==0 but pes_buffer doesn't exists: just wait for the first PUSI==1 packet...
                    return TRUE;
                }           
            }
            
        } else {
            GST_ERROR("Adaptation field exists, currently cannot handle this case...");
            return FALSE;
        }
    }    
    clear_txt_buffer(txt);
    return FALSE;
}


