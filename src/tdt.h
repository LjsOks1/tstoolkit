#ifndef TDT_H
#define TDT_H

gboolean get_tdt_time(GDateTime*,guint8*);

typedef struct TDT {
    gboolean tdt_defined;
    guint cc;
    guint8 tdt_packet[188];
} TDT;

gboolean construct_tdt(TDT*);
gboolean get_tdt_packet(TDT*,guint8**);


typedef struct TOT {
    gboolean tot_defined;
    guint cc;
    gchar *country_code;
    guint8 country_region_id,local_time_offset_polarity;
    guint16 local_time_offset;
    gchar *time_of_change;
    guint16 next_time_offset;
    guint8 tot_packet[188];
} TOT;

gboolean construct_tot(TOT*);
gboolean get_tot_packet(TOT*,guint8**);


#endif
