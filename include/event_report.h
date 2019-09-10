/**
 * @file    event_report.h
 * @ingroup event_report
 * @author  Roland Ottensamer (roland.ottensamer@univie.ac.at),
 *          Armin Luntzer (roland.ottensamer@univie.ac.at)
 * @date    August, 2015
 *
 * @copyright 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */



#ifndef EVENT_REPORT__H
#define EVENT_REPORT__H


#include <stdint.h>

#if (__sparc__) && !NO_IASW
#include <CrIaIasw.h>
#include <Services/General/CrIaConstants.h>
#else

#define CRIA_SERV5_EVT_NORM		  1
#define CRIA_SERV5_EVT_ERR_LOW_SEV	  2
#define CRIA_SERV5_EVT_ERR_MED_SEV	  3
#define CRIA_SERV5_EVT_ERR_HIGH_SEV	  4
#define CRIA_SERV5_EVT_INIT_SUCC	301
#define CRIA_SERV5_EVT_INIT_FAIL	302
#define CRIA_SERV5_EVT_NOTIF_ERR	304
#define CRIA_SERV5_EVT_SPW_ERR		300
#define CRIA_SERV5_EVT_SPW_ERR_L	315
#define CRIA_SERV5_EVT_SPW_ERR_M	316
#define CRIA_SERV5_EVT_SPW_ERR_H	317
#define CRIA_SERV5_EVT_1553_ERR		310
#define CRIA_SERV5_EVT_1553_ERR_L	311
#define CRIA_SERV5_EVT_1553_ERR_M	312
#define CRIA_SERV5_EVT_1553_ERR_H	313
#define CRIA_SERV5_EVT_FL_FBF_ERR	325
#define CRIA_SERV5_EVT_SBIT_ERR		330
#define CRIA_SERV5_EVT_DBIT_ERR		335
#define CRIA_SERV5_EVT_FL_EL_ERR	320
#define CRIA_SERV5_EVT_SYNC_LOSS	350

void CrIaEvtRaise(uint8_t subtype, uint16_t id, uint16_t *data, uint32_t size);

#endif


enum error_class {INIT, NOTIFY, FBF, EDAC, CORE1553BRM, GRSPW2, ERRLOG, SYNC};
enum error_severity {NORMAL, LOW, MEDIUM, HIGH};

void event_report(enum error_class c, enum error_severity s, uint32_t err);

#endif

