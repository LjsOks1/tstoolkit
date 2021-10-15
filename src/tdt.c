#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "tdt.h"
#include "scte35.h"

GST_DEBUG_CATEGORY_EXTERN(ts_parser);
#define GST_CAT_DEFAULT ts_parser


gboolean construct_tdt(TDT* tdt)
{
  guint8* tdt_packet=tdt->tdt_packet;
  memset(tdt_packet,0xff,188);
  tdt_packet[0]=0x47; tdt_packet[1]=0x40; tdt_packet[2]=0x14; tdt_packet[3]=0x1d;
  tdt_packet[4]=0x00; tdt_packet[5]=0x70; tdt_packet[6]=0x70; tdt_packet[7]=0x05;

  return TRUE;
}
/* Doesn't move the original p pointer!!! */
gboolean get_tdt_time(GDateTime *dt,guint8* p)
{
  guint mjd;
  if(g_date_time_get_month(dt))
    mjd=14956+g_date_time_get_day_of_month(dt)+((g_date_time_get_year(dt)-1900)*365.25)+((g_date_time_get_month(dt)+1)*30.6001);
  else
    mjd=14956+g_date_time_get_day_of_month(dt)+((g_date_time_get_year(dt)-1-1900)*365.25)+((g_date_time_get_month(dt)+13)*306001);
  GST_WRITE_UINT16_BE(p,mjd&0xffff);
  p+=2;
  *(p++)=((g_date_time_get_hour(dt)/10)<<4) | (g_date_time_get_hour(dt)%10);
  *(p++)=((g_date_time_get_minute(dt)/10)<<4) | (g_date_time_get_minute(dt)%10);
  *(p++)=((g_date_time_get_second(dt)/10)<<4) | (g_date_time_get_second(dt)%10);
  return TRUE;
}


gboolean get_tdt_packet(TDT* tdt,guint8 **p) 
{
  guint8* tdt_packet=tdt->tdt_packet;
  GDateTime *utc=g_date_time_new_now_utc();
  guint mjd;
  *p=g_memdup(tdt_packet,188);
  (*p)[3]=0x10|((tdt->cc++)%16); //CC must be increased every time....
  if(g_date_time_get_month(utc)>2)
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1900)*365.25)+((g_date_time_get_month(utc)+1)*30.6001);
  else
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1-1900)*365.25)+((g_date_time_get_month(utc)+13)*306001);
  GST_WRITE_UINT16_BE(&(*p)[8],mjd&0xffff);
  (*p)[10]=((g_date_time_get_hour(utc)/10)<<4) | (g_date_time_get_hour(utc)%10);
  (*p)[11]=((g_date_time_get_minute(utc)/10)<<4) | (g_date_time_get_minute(utc)%10);
  (*p)[12]=((g_date_time_get_second(utc)/10)<<4) | (g_date_time_get_second(utc)%10);
  g_date_time_unref(utc);
  return TRUE;
}

gboolean construct_tot(TOT* tot)
{
  GDateTime *utc=g_date_time_new_from_iso8601(tot->time_of_change,NULL);
  guint8* tot_packet=tot->tot_packet;
  guint mjd;
  memset(tot_packet,0xff,188);
  tot_packet[0]=0x47; tot_packet[1]=0x40; tot_packet[2]=0x14; tot_packet[3]=0x1d;
  tot_packet[4]=0x00; tot_packet[5]=0x73; tot_packet[6]=0x70; tot_packet[7]=0x1a;
  // Next 5 bytes are for UTC
  tot_packet[13]=0xf0; tot_packet[14]=0x0f; tot_packet[15]=0x58; tot_packet[16]=0x0d;
  memcpy(&tot_packet[17],tot->country_code,3);
  tot_packet[20]=tot->country_region_id<<2 | 1<<1 | tot->local_time_offset_polarity;
  GST_WRITE_UINT16_BE(&tot_packet[21],tot->local_time_offset);
  if(g_date_time_get_month(utc)>2)
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1900)*365.25)+((g_date_time_get_month(utc)+1)*30.6001);
  else
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1-1900)*365.25)+((g_date_time_get_month(utc)+13)*306001);
  GST_WRITE_UINT16_BE(&tot_packet[23],mjd&0xffff);
  tot_packet[25]=((g_date_time_get_hour(utc)/10)<<4) | (g_date_time_get_hour(utc)%10);
  tot_packet[26]=((g_date_time_get_minute(utc)/10)<<4) | (g_date_time_get_minute(utc)%10);
  tot_packet[27]=((g_date_time_get_second(utc)/10)<<4) | (g_date_time_get_second(utc)%10);
  GST_WRITE_UINT16_BE(&tot_packet[28],tot->next_time_offset);
  g_date_time_unref(utc);
  return TRUE;
}

gboolean get_tot_packet(TOT* tot,guint8 **p) 
{
  guint8* tot_packet=tot->tot_packet;
  GDateTime *utc=g_date_time_new_now_utc();
  guint mjd;
  
  *p=g_memdup(tot_packet,188);
  (*p)[3]=0x10|((tot->cc++)%16); //CC must be increased every time....
  if(g_date_time_get_month(utc)>2)
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1900)*365.25)+((g_date_time_get_month(utc)+1)*30.6001);
  else
    mjd=14956+g_date_time_get_day_of_month(utc)+((g_date_time_get_year(utc)-1-1900)*365.25)+((g_date_time_get_month(utc)+13)*306001);
  GST_WRITE_UINT16_BE(&(*p)[8],mjd&0xffff);
  (*p)[10]=((g_date_time_get_hour(utc)/10)<<4) | (g_date_time_get_hour(utc)%10);
  (*p)[11]=((g_date_time_get_minute(utc)/10)<<4) | (g_date_time_get_minute(utc)%10);
  (*p)[12]=((g_date_time_get_second(utc)/10)<<4) | (g_date_time_get_second(utc)%10);
  GST_WRITE_UINT32_BE (&(*p)[30], _calc_crc32 (&(*p)[5], 25));
  g_date_time_unref(utc);

  return TRUE;
}

