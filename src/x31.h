#ifndef X31_H
#define X31_H

#include <gmodule.h>
#include "scte35.h"

typedef struct TXT {
    GArray *pes_buffer;    //raw txt packet payloads aggregated through ts packets
    guint64 PTS;                //PTS of the PES we collect the packets from
    guint PES_packet_length;    //not that useful.....
    guint8 cc;                  //continuity counter
    guint8 splice_in,splice_out;
    guint64 preroll;
} TXT;


TXT* new_txt_buffer();
gboolean clear_txt_buffer(TXT*);
gboolean push_txt_packet(guint8*, GArray*, TXT*,SCTE35*);
gboolean process_x31(guint8*, GArray* ,SCTE35*);
 
#endif
