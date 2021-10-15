#ifndef CONFIG_H
#define CONFIG_H

#include "x31.h"
#include "scte35.h"
#include "ait.h"
#include "pmt.h"
#include "tdt.h"

typedef struct Settings
{
    gchar *src_pipeline,*sink_pipeline,*base_url,*location;
    int pmt_pid,txt_pid,scte35_pid,ait_pid;
    gboolean has_ait,has_scte35,has_tdt;
} Settings;


typedef struct ProgramData
{
    GMainLoop* loop;
    GstElement* source;
    GstElement* sink;
    guint8 pid20_cc;
    TXT *txt;
    SCTE35 *scte35;
    AIT *ait;
    PMT *pmt;
    TDT *tdt;
    TOT *tot;
} ProgramData;

#endif
