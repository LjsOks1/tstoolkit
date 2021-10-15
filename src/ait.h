#ifndef AIT_H
#define AIT_H

typedef struct GstMpegtsApplication {
    guint organization_id,application_id;
    guint8 application_control_code;
    GPtrArray *descriptors;
} GstMpegtsApplication;

typedef struct GstMpegtsAIT {
    guint application_type;
    guint8 test_application_flag,last_section_number,version_number,section_number;
    GPtrArray *common_descriptors;
    GPtrArray *applications;
} GstMpegtsAIT;

typedef struct AIT {
    gboolean ait_defined;
    guint cc;
    guint16 ait_pid;
    gchar *url_base,*app_name;
    gchar *initial_path,*ait_stream;
    GstMpegtsAIT *mpegtsAIT;
    guint8 ts_packet[188]; //a ready made ts packet that can directly go to the transport stream
} AIT;

gboolean construct_ait(AIT*);
#endif
