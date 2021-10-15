#ifndef SCTE35_H
#define SCTE35_H


#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)
 
typedef struct SCTE35 
{
    guint8 cc;
    guint16 scte35_pid;
    guint16 txt_pid;
    gboolean scte35_defined;
} SCTE35;

guint32 _calc_crc32 (const guint8* , guint );
void _packetize_descriptor_array (GPtrArray * , guint8 ** );
gboolean add_splice(guint8,GArray*,SCTE35*,guint32,guint64); 

#endif
