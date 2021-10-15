#ifndef  PMT_H
#define PMT_H


typedef struct PMT {
    gboolean pmt_defined;                     // don't process PMT if it is not defined in the config file
    guint8* raw_section_in;                   // raw incoming section
    guint8* raw_section_out;                  // raw outgoing section
    guint8 pmt_version_in;                    // version of the last parsed incoming PMT section
    guint8 pmt_version_out;                   // random version_id on the outgoing pmt
    guint16 raw_section_in_length;            // incoming PMT raw section length
    guint16 raw_section_out_length;           // outgoing PMT raw section length
    guint16 missing_bytes_from_section_in;    // missing bytes from the incoming section before it is complete
    guint8 cc_out;                            // countinuity counter for output PMT packets
    guint16 raw_section_out_start_index;      // start index of the section data that goes to the next TS packet;
    guint16 pmt_pid;                          // PMT Pid
} PMT;

gboolean push_pmt_packet(guint8 *ptr, GArray *packet_array, void* );

#endif

