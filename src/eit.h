#ifndef EIT_H
#define EIT_H

//GstMpegtsSection * _gst_mpegts_section_init (guint16 pid, guint8 table_id);

GstMpegtsEIT* gst_mpegts_eit_new(guint8);
GstMpegtsEITEvent* gst_mpegts_eit_event_new();
GstMpegtsSection* gst_mpegts_section_from_eit(GstMpegtsEIT*,guint16);
GstMpegtsDescriptor* create_short_event_descriptor(char*,char*,char*);
#endif
