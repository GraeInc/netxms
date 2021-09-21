/*
** NetXMS - Network Management System
** Copyright (C) 2003-2021 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: nxnet.h
**
**/

#ifndef _nxnet_h_
#define _nxnet_h_

#ifdef __HP_aCC
#pragma pack 1
#else
#pragma pack(1)
#endif

/**
 * IP Header -- RFC 791
 */
struct IPHDR
{
   BYTE m_cVIHL;           // Version and IHL
   BYTE m_cTOS;            // Type Of Service
   uint16_t m_wLen;        // Total Length
   uint16_t m_wId;         // Identification
   uint16_t m_wFlagOff;    // Flags and Fragment Offset
   BYTE m_cTTL;            // Time To Live
   BYTE m_cProtocol;       // Protocol
   uint16_t m_wChecksum;   // Checksum
   struct in_addr m_iaSrc; // Internet Address - Source
   struct in_addr m_iaDst; // Internet Address - Destination
};

/**
 * ICMP Header - RFC 792
 */
struct ICMPHDR
{
   BYTE m_cType;            // Type
   BYTE m_cCode;            // Code
   uint16_t m_wChecksum;    // Checksum
   uint16_t m_wId;          // Identification
   uint16_t m_wSeq;         // Sequence
};

/**
 * Max ping size
 */
#define MAX_PING_SIZE	8192

/**
 * ICMP echo request structure
 */
struct ICMP_ECHO_REQUEST
{
   ICMPHDR m_icmpHdr;
   BYTE m_data[MAX_PING_SIZE - sizeof(ICMPHDR) - sizeof(IPHDR)];
};

/**
 * ICMP echo reply structure
 */
struct ICMP_ECHO_REPLY
{
   IPHDR m_ipHdr;
   ICMPHDR m_icmpHdr;
   BYTE m_data[MAX_PING_SIZE - sizeof(ICMPHDR) - sizeof(IPHDR)];
};

/**
 * Combined IPv6 + ICMPv6 header for checksum calculation
 */
struct ICMP6_PACKET_HEADER
{
   // IPv6 header
   BYTE srcAddr[16];
   BYTE destAddr[16];
   uint32_t length;
   BYTE padding[3];
   BYTE nextHeader;

   // ICMPv6 header
   BYTE type;
   BYTE code;
   uint16_t checksum;

   // Custom fields
   uint32_t id;
   uint32_t sequence;
   BYTE data[8];      // actual length may differ
};

/**
 * ICMPv6 reply header
 */
struct ICMP6_REPLY
{
   // ICMPv6 header
   BYTE type;
   BYTE code;
   uint16_t checksum;

   // Custom fields
   uint32_t id;
   uint32_t sequence;
};

/**
 * ICMPv6 error report structure
 */
struct ICMP6_ERROR_REPORT
{
   // ICMPv6 header
   BYTE type;
   BYTE code;
   uint16_t checksum;

   // Custom fields
   uint32_t unused;
   BYTE ipv6hdr[8];
   BYTE srcAddr[16];
   BYTE destAddr[16];
};

#ifdef __HP_aCC
#pragma pack
#else
#pragma pack()
#endif

/**
 * Interface types
 */
#define IFTYPE_OTHER                      1
#define IFTYPE_REGULAR1822                2
#define IFTYPE_HDH1822                    3
#define IFTYPE_DDN_X25                    4
#define IFTYPE_RFC877_X25                 5
#define IFTYPE_ETHERNET_CSMACD            6
#define IFTYPE_ISO88023_CSMACD            7
#define IFTYPE_ISO88024_TOKENBUS          8
#define IFTYPE_ISO88025_TOKENRING         9
#define IFTYPE_ISO88026_MAN               10
#define IFTYPE_STARLAN                    11
#define IFTYPE_PROTEON_10MBIT             12
#define IFTYPE_PROTEON_80MBIT             13
#define IFTYPE_HYPERCHANNEL               14
#define IFTYPE_FDDI                       15
#define IFTYPE_LAPB                       16
#define IFTYPE_SDLC                       17
#define IFTYPE_DS1                        18
#define IFTYPE_E1                         19
#define IFTYPE_BASIC_ISDN                 20
#define IFTYPE_PRIMARY_ISDN               21
#define IFTYPE_PROP_PTP_SERIAL            22
#define IFTYPE_PPP                        23
#define IFTYPE_SOFTWARE_LOOPBACK          24
#define IFTYPE_EON                        25
#define IFTYPE_ETHERNET_3MBIT             26
#define IFTYPE_NSIP                       27
#define IFTYPE_SLIP                       28
#define IFTYPE_ULTRA                      29
#define IFTYPE_DS3                        30
#define IFTYPE_SMDS                       31
#define IFTYPE_FRAME_RELAY                32
#define IFTYPE_RS232                      33
#define IFTYPE_PARA                       34
#define IFTYPE_ARCNET                     35
#define IFTYPE_ARCNET_PLUS                36
#define IFTYPE_ATM                        37
#define IFTYPE_MIOX25                     38
#define IFTYPE_SONET                      39
#define IFTYPE_X25PLE                     40
#define IFTYPE_ISO88022LLC                41
#define IFTYPE_LOCALTALK                  42
#define IFTYPE_SMDS_DXI                   43
#define IFTYPE_FRAME_RELAY_SERVICE        44
#define IFTYPE_V35                        45
#define IFTYPE_HSSI                       46
#define IFTYPE_HIPPI                      47
#define IFTYPE_MODEM                      48
#define IFTYPE_AAL5                       49
#define IFTYPE_SONET_PATH                 50
#define IFTYPE_SONET_VT                   51
#define IFTYPE_SMDS_ICIP                  52
#define IFTYPE_PROP_VIRTUAL               53
#define IFTYPE_PROP_MULTIPLEXOR           54
#define IFTYPE_IEEE80212                  55
#define IFTYPE_FIBRECHANNEL               56
#define IFTYPE_HIPPIINTERFACE             57
#define IFTYPE_FRAME_RELAY_INTERCONNECT   58
#define IFTYPE_AFLANE8023                 59
#define IFTYPE_AFLANE8025                 60
#define IFTYPE_CCTEMUL                    61
#define IFTYPE_FAST_ETHERNET              62
#define IFTYPE_ISDN                       63
#define IFTYPE_V11                        64
#define IFTYPE_V36                        65
#define IFTYPE_G703_AT64K                 66
#define IFTYPE_G703_AT2MB                 67
#define IFTYPE_QLLC                       68
#define IFTYPE_FASTETHERFX                69
#define IFTYPE_CHANNEL                    70
#define IFTYPE_IEEE80211                  71
#define IFTYPE_IBM370_PARCHAN             72
#define IFTYPE_ESCON                      73
#define IFTYPE_DLSW                       74
#define IFTYPE_ISDNS                      75
#define IFTYPE_ISDNU                      76
#define IFTYPE_LAPD                       77
#define IFTYPE_IPSWITCH                   78
#define IFTYPE_RSRB                       79
#define IFTYPE_ATMLOGICAL                 80
#define IFTYPE_DS0                        81
#define IFTYPE_DS0_BUNDLE                 82
#define IFTYPE_BSC                        83
#define IFTYPE_ASYNC                      84
#define IFTYPE_CNR                        85
#define IFTYPE_ISO88025DTR                86
#define IFTYPE_EPLRS                      87
#define IFTYPE_ARAP                       88
#define IFTYPE_PROPCNLS                   89
#define IFTYPE_HOSTPAD                    90
#define IFTYPE_TERMPAD                    91
#define IFTYPE_FRAME_RELAY_MPI            92
#define IFTYPE_X213                       93
#define IFTYPE_ADSL                       94
#define IFTYPE_RADSL                      95
#define IFTYPE_SDSL                       96
#define IFTYPE_VDSL                       97
#define IFTYPE_ISO88025CRFPINT            98
#define IFTYPE_MYRINET                    99
#define IFTYPE_VOICEEM                    100
#define IFTYPE_VOICEFXO                   101
#define IFTYPE_VOICEFXS                   102
#define IFTYPE_VOICEENCAP                 103
#define IFTYPE_VOICEOVERIP                104
#define IFTYPE_ATMDXI                     105
#define IFTYPE_ATMFUNI                    106
#define IFTYPE_ATMIMA                     107
#define IFTYPE_PPPMULTILINKBUNDLE         108
#define IFTYPE_IPOVERCDLC                 109
#define IFTYPE_IPOVERCLAW                 110
#define IFTYPE_STACKTOSTACK               111
#define IFTYPE_VIRTUAL_IP_ADDRESS         112
#define IFTYPE_MPC                        113
#define IFTYPE_IPOVERATM                  114
#define IFTYPE_ISO88025FIBER              115
#define IFTYPE_TDLC                       116
#define IFTYPE_GIGABIT_ETHERNET           117
#define IFTYPE_HDLC                       118
#define IFTYPE_LAPF                       119
#define IFTYPE_V37                        120
#define IFTYPE_X25MLP                     121
#define IFTYPE_X25_HUNT_GROUP             122
#define IFTYPE_TRANSPHDLC                 123
#define IFTYPE_INTERLEAVE                 124
#define IFTYPE_FAST                       125
#define IFTYPE_IP                         126
#define IFTYPE_DOCSCABLE_MACLAYER         127
#define IFTYPE_DOCSCABLE_DOWNSTREAM       128
#define IFTYPE_DOCSCABLE_UPSTREAM         129
#define IFTYPE_A12MPPSWITCH               130
#define IFTYPE_TUNNEL                     131
#define IFTYPE_COFFEE                     132
#define IFTYPE_CES                        133
#define IFTYPE_ATM_SUBINTERFACE           134
#define IFTYPE_L2VLAN                     135
#define IFTYPE_L3IPVLAN                   136
#define IFTYPE_L3IPXVLAN                  137
#define IFTYPE_DIGITAL_POWERLINE          138
#define IFTYPE_MEDIAMAIL_OVER_IP          139
#define IFTYPE_DTM                        140
#define IFTYPE_DCN                        141
#define IFTYPE_IPFORWARD                  142
#define IFTYPE_MSDSL                      143
#define IFTYPE_IEEE1394                   144
#define IFTYPE_GSN                        145
#define IFTYPE_DVBRCC_MACLAYER            146
#define IFTYPE_DVBRCC_DOWNSTREAM          147
#define IFTYPE_DVBRCC_UPSTREAM            148
#define IFTYPE_ATM_VIRTUAL                149
#define IFTYPE_MPLS_TUNNEL                150
#define IFTYPE_SRP                        151
#define IFTYPE_VOICE_OVER_ATM             152
#define IFTYPE_VOICE_OVER_FRAME_RELAY     153
#define IFTYPE_IDSL                       154
#define IFTYPE_COMPOSITE_LINK             155
#define IFTYPE_SS7_SIGLINK                156
#define IFTYPE_PROPWIRELESSP2P            157
#define IFTYPE_FRFORWARD                  158
#define IFTYPE_RFC1483                    159
#define IFTYPE_USB                        160
#define IFTYPE_IEEE8023ADLAG              161
#define IFTYPE_BGP_POLICY_ACCOUNTING      162
#define IFTYPE_FRF16MFR_BUNDLE            163
#define IFTYPE_H323_GATEKEEPER            164
#define IFTYPE_H323_PROXY                 165
#define IFTYPE_MPLS                       166
#define IFTYPE_MFSIGLINK                  167
#define IFTYPE_HDSL2                      168
#define IFTYPE_SHDSL                      169
#define IFTYPE_DS1FDL                     170
#define IFTYPE_POS                        171
#define IFTYPE_DVBASI_IN                  172
#define IFTYPE_DVBASI_OUT                 173
#define IFTYPE_PLC                        174
#define IFTYPE_NFAS                       175
#define IFTYPE_TR008                      176
#define IFTYPE_GR303RDT                   177
#define IFTYPE_GR303IDT                   178
#define IFTYPE_ISUP                       179
#define IFTYPE_PROPDOCSWIRELESSMACLAYER   180
#define IFTYPE_PROPDOCSWIRELESSDOWNSTREAM 181
#define IFTYPE_PROPDOCSWIRELESSUPSTREAM   182
#define IFTYPE_HIPERLAN2                  183
#define IFTYPE_PROPBWAP2MP                184
#define IFTYPE_SONET_OVERHEAD_CHANNEL     185
#define IFTYPE_DW_OVERHEAD_CHANNEL        186
#define IFTYPE_AAL2                       187
#define IFTYPE_RADIOMAC                   188
#define IFTYPE_ATMRADIO                   189
#define IFTYPE_IMT                        190
#define IFTYPE_MVL                        191
#define IFTYPE_REACHDSL                   192
#define IFTYPE_FRDLCIENDPT                193
#define IFTYPE_ATMVCIENDPT                194
#define IFTYPE_OPTICAL_CHANNEL            195
#define IFTYPE_OPTICAL_TRANSPORT          196
#define IFTYPE_PROPATM                    197
#define IFTYPE_VOICE_OVER_CABLE           198
#define IFTYPE_INFINIBAND                 199
#define IFTYPE_TELINK                     200
#define IFTYPE_Q2931                      201
#define IFTYPE_VIRTUALTG                  202
#define IFTYPE_SIPTG                      203
#define IFTYPE_SIPSIG                     204
#define IFTYPE_DOCSCABLEUPSTREAMCHANNEL   205
#define IFTYPE_ECONET                     206
#define IFTYPE_PON155                     207
#define IFTYPE_PON622                     208
#define IFTYPE_BRIDGE                     209
#define IFTYPE_LINEGROUP                  210
#define IFTYPE_VOICEEMFGD                 211
#define IFTYPE_VOICEFGDEANA               212
#define IFTYPE_VOICEDID                   213
#define IFTYPE_MPEG_TRANSPORT             214
#define IFTYPE_SIXTOFOUR                  215
#define IFTYPE_GTP                        216
#define IFTYPE_PDNETHERLOOP1              217
#define IFTYPE_PDNETHERLOOP2              218
#define IFTYPE_OPTICAL_CHANNEL_GROUP      219
#define IFTYPE_HOMEPNA                    220
#define IFTYPE_GFP                        221
#define IFTYPE_CISCO_ISL_VLAN             222
#define IFTYPE_ACTELIS_METALOOP           223
#define IFTYPE_FCIPLINK                   224
#define IFTYPE_RPR                        225
#define IFTYPE_QAM                        226
#define IFTYPE_LMP                        227
#define IFTYPE_CBLVECTASTAR               228
#define IFTYPE_DOCSCABLEMCMTSDOWNSTREAM   229
#define IFTYPE_ADSL2                      230
#define IFTYPE_MACSECCONTROLLEDIF         231
#define IFTYPE_MACSECUNCONTROLLEDIF       232
#define IFTYPE_AVICIOPTICALETHER          233
#define IFTYPE_ATM_BOND                   234
#define IFTYPE_VOICEFGDOS                 235
#define IFTYPE_MOCA_VERSION1              236
#define IFTYPE_IEEE80216WMAN              237
#define IFTYPE_ADSL2PLUS                  238
#define IFTYPE_DVBRCSMACLAYER             239
#define IFTYPE_DVBTDM                     240
#define IFTYPE_DVBRCSTDMA                 241
#define IFTYPE_X86LAPS                    242
#define IFTYPE_WWANPP                     243
#define IFTYPE_WWANPP2                    244
#define IFTYPE_VOICEEBS                   245
#define IFTYPE_IFPWTYPE                   246
#define IFTYPE_ILAN                       247
#define IFTYPE_PIP                        248
#define IFTYPE_ALUELP                     249
#define IFTYPE_GPON                       250
#define IFTYPE_VDSL2                      251
#define IFTYPE_CAPWAP_DOT11_PROFILE       252
#define IFTYPE_CAPWAP_DOT11_BSS           253
#define IFTYPE_CAPWAP_WTP_VIRTUAL_RADIO   254
#define IFTYPE_BITS                       255
#define IFTYPE_DOCSCABLEUPSTREAMRFPORT    256
#define IFTYPE_CABLEDOWNSTREAMRFPORT      257
#define IFTYPE_VMWARE_VIRTUAL_NIC         258
#define IFTYPE_IEEE802154                 259
#define IFTYPE_OTNODU                     260
#define IFTYPE_OTNOTU                     261
#define IFTYPE_IFVFITYPE                  262
#define IFTYPE_G9981                      263
#define IFTYPE_G9982                      264
#define IFTYPE_G9983                      265
#define IFTYPE_ALUEPON                    266
#define IFTYPE_ALUEPONONU                 267
#define IFTYPE_ALUEPONPHYSICALUNI         268
#define IFTYPE_ALUEPONLOGICALLINK         269
#define IFTYPE_ALUGPONONU                 270
#define IFTYPE_ALUGPONPHYSICALUNI         271
#define IFTYPE_VMWARE_NIC_TEAM            272

#endif   /* _nxnet_h_ */
