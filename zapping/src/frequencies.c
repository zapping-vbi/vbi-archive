/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/**
 * The frequency table is taken from xawtv with donations around the globe.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "strnatcmp.h"
#include "frequencies.h"

/* --------------------------------------------------------------------- */

/* US broadcast */
static tveng_channel ntsc_bcast[] = {
    { "2",	 55250 },
    { "3",	 61250 },
    { "4",	 67250 },
    { "5",	 77250 },
    { "6",	 83250 },
    { "7",	175250 },
    { "8",	181250 },
    { "9",	187250 },
    { "10",	193250 },
    { "11",	199250 },
    { "12",	205250 },
    { "13",	211250 },
    { "14",	471250 },
    { "15",	477250 },
    { "16",	483250 },
    { "17",	489250 },
    { "18",	495250 },
    { "19",	501250 },
    { "20",	507250 },
    { "21",	513250 },
    { "22",	519250 },
    { "23",	525250 },
    { "24",	531250 },
    { "25",	537250 },
    { "26",	543250 },
    { "27",	549250 },
    { "28",	555250 },
    { "29",	561250 },
    { "30",	567250 },
    { "31",	573250 },
    { "32",	579250 },
    { "33",	585250 },
    { "34",	591250 },
    { "35",	597250 },
    { "36",	603250 },
    { "37",	609250 },
    { "38",	615250 },
    { "39",	621250 },
    { "40",	627250 },
    { "41",	633250 },
    { "42",	639250 },
    { "43",	645250 },
    { "44",	651250 },
    { "45",	657250 },
    { "46",	663250 },
    { "47",	669250 },
    { "48",	675250 },
    { "49",	681250 },
    { "50",	687250 },
    { "51",	693250 },
    { "52",	699250 },
    { "53",	705250 },
    { "54",	711250 },
    { "55",	717250 },
    { "56",	723250 },
    { "57",	729250 },
    { "58",	735250 },
    { "59",	741250 },
    { "60",	747250 },
    { "61",	753250 },
    { "62",	759250 },
    { "63",	765250 },
    { "64",	771250 },
    { "65",	777250 },
    { "66",	783250 },
    { "67",	789250 },
    { "68",	795250 },
    { "69",	801250 },
    { "70",	807250 },
    { "71",	813250 },
    { "72",	819250 },
    { "73",	825250 },
    { "74",	831250 },
    { "75",	837250 },
    { "76",	843250 },
    { "77",	849250 },
    { "78",	855250 },
    { "79",	861250 },
    { "80",	867250 },
    { "81",	873250 },
    { "82",	879250 },
    { "83",	885250 },
};

/* US cable */
static tveng_channel ntsc_cable[] = {
    { "1",	 73250 },
    { "2",	 55250 },
    { "3",	 61250 },
    { "4",	 67250 },
    { "5",	 77250 },
    { "6",	 83250 },
    { "7",	175250 },
    { "8",	181250 },
    { "9",	187250 },
    { "10",	193250 },
    { "11",	199250 },
    { "12",	205250 },
    { "13",	211250 },
    { "14",	121250 },
    { "15",	127250 },
    { "16",	133250 },
    { "17",	139250 },
    { "18",	145250 },
    { "19",	151250 },
    { "20",	157250 },
    { "21",	163250 },
    { "22",	169250 },
    { "23",	217250 },
    { "24",	223250 },
    { "25",	229250 },
    { "26",	235250 },
    { "27",	241250 },
    { "28",	247250 },
    { "29",	253250 },
    { "30",	259250 },
    { "31",	265250 },
    { "32",	271250 },
    { "33",	277250 },
    { "34",	283250 },
    { "35",	289250 },
    { "36",	295250 },
    { "37",	301250 },
    { "38",	307250 },
    { "39",	313250 },
    { "40",	319250 },
    { "41",	325250 },
    { "42",	331250 },
    { "43",	337250 },
    { "44",	343250 },
    { "45",	349250 },
    { "46",	355250 },
    { "47",	361250 },
    { "48",	367250 },
    { "49",	373250 },
    { "50",	379250 },
    { "51",	385250 },
    { "52",	391250 },
    { "53",	397250 },
    { "54",	403250 },
    { "55",	409250 },
    { "56",	415250 },
    { "57",	421250 },
    { "58",	427250 },
    { "59",	433250 },
    { "60",	439250 },
    { "61",	445250 },
    { "62",	451250 },
    { "63",	457250 },
    { "64",	463250 },
    { "65",	469250 },
    { "66",	475250 },
    { "67",	481250 },
    { "68",	487250 },
    { "69",	493250 },
    { "70",	499250 },
    { "71",	505250 },
    { "72",	511250 },
    { "73",	517250 },
    { "74",	523250 },
    { "75",	529250 },
    { "76",	535250 },
    { "77",	541250 },
    { "78",	547250 },
    { "79",	553250 },
    { "80",	559250 },
    { "81",	565250 },
    { "82",	571250 },
    { "83",	577250 },
    { "84",	583250 },
    { "85",	589250 },
    { "86",	595250 },
    { "87",	601250 },
    { "88",	607250 },
    { "89",	613250 },
    { "90",	619250 },
    { "91",	625250 },
    { "92",	631250 },
    { "93",	637250 },
    { "94",	643250 },
    { "95",	 91250 },
    { "96",	 97250 },
    { "97",	103250 },
    { "98",	109250 },
    { "99",	115250 },
    { "100",	649250 },
    { "101",	655250 },
    { "102",	661250 },
    { "103",	667250 },
    { "104",	673250 },
    { "105",	679250 },
    { "106",	685250 },
    { "107",	691250 },
    { "108",	697250 },
    { "109",	703250 },
    { "110",	709250 },
    { "111",	715250 },
    { "112",	721250 },
    { "113",	727250 },
    { "114",	733250 },
    { "115",	739250 },
    { "116",	745250 },
    { "117",	751250 },
    { "118",	757250 },
    { "119",	763250 },
    { "120",	769250 },
    { "121",	775250 },
    { "122",	781250 },
    { "123",	787250 },
    { "124",	793250 },
    { "125",	799250 },

    { "T7", 	  8250 },
    { "T8",	 14250 },
    { "T9",	 20250 },
    { "T10",	 26250 },
    { "T11",	 32250 },
    { "T12",	 38250 },
    { "T13",	 44250 },
    { "T14",	 50250 }
};

/* US HRC */
static tveng_channel ntsc_hrc[] = {
    { "1",	  72000 },
    { "2",	  54000 }, 
    { "3",	  60000 }, 
    { "4",	  66000 }, 
    { "5",	  78000 }, 
    { "6",	  84000 }, 
    { "7",	 174000 },
    { "8",	 180000 },
    { "9",	 186000 },
    { "10",	 192000 },
    { "11",	 198000 },
    { "12",	 204000 },
    { "13",	 210000 },
    { "14",	 120000 },
    { "15",	 126000 },
    { "16",	 132000 },
    { "17",	 138000 },
    { "18",	 144000 },
    { "19",	 150000 },
    { "20",	 156000 },
    { "21",	 162000 },
    { "22",	 168000 },
    { "23",	 216000 },
    { "24",	 222000 },
    { "25",	 228000 },
    { "26",	 234000 },
    { "27",	 240000 },
    { "28",	 246000 },
    { "29",	 252000 },
    { "30",	 258000 },
    { "31",	 264000 },
    { "32",	 270000 },
    { "33",	 276000 },
    { "34",	 282000 },
    { "35",	 288000 },
    { "36",	 294000 },
    { "37",	 300000 },
    { "38",	 306000 },
    { "39",	 312000 },
    { "40",	 318000 },
    { "41",	 324000 },
    { "42",	 330000 },
    { "43",	 336000 },
    { "44",	 342000 },
    { "45",	 348000 },
    { "46",	 354000 },
    { "47",	 360000 },
    { "48",	 366000 },
    { "49",	 372000 },
    { "50",	 378000 },
    { "51",	 384000 },
    { "52",	 390000 },
    { "53",	 396000 },
    { "54",	 402000 },
    { "55",	 408000 },
    { "56",	 414000 },
    { "57",	 420000 },
    { "58",	 426000 },
    { "59",	 432000 },
    { "60",	 438000 },
    { "61",	 444000 },
    { "62",	 450000 },
    { "63",	 456000 },
    { "64",	 462000 },
    { "65",	 468000 },
    { "66",	 474000 },
    { "67",	 480000 },
    { "68",	 486000 },
    { "69",	 492000 },
    { "70",	 498000 },
    { "71",	 504000 },
    { "72",	 510000 },
    { "73",	 516000 },
    { "74",	 522000 },
    { "75",	 528000 },
    { "76",	 534000 },
    { "77",	 540000 },
    { "78",	 546000 },
    { "79",	 552000 },
    { "80",	 558000 },
    { "81",	 564000 },
    { "82",	 570000 },
    { "83",	 576000 },
    { "84",	 582000 },
    { "85",	 588000 },
    { "86",	 594000 },
    { "87",	 600000 },
    { "88",	 606000 },
    { "89",	 612000 },
    { "90",	 618000 },
    { "91",	 624000 },
    { "92",	 630000 },
    { "93",	 636000 },
    { "94",	 642000 },
    { "95",	 900000 },
    { "96",	 960000 },
    { "97",	 102000 },
    { "98",	 108000 },
    { "99",	 114000 },
    { "100",	 648000 },
    { "101",	 654000 },
    { "102",	 660000 },
    { "103",	 666000 },
    { "104",	 672000 },
    { "105",	 678000 },
    { "106",	 684000 },
    { "107",	 690000 },
    { "108",	 696000 },
    { "109",	 702000 },
    { "110",	 708000 },
    { "111",	 714000 },
    { "112",	 720000 },
    { "113",	 726000 },
    { "114",	 732000 },
    { "115",	 738000 },
    { "116",	 744000 },
    { "117",	 750000 },
    { "118",	 756000 },
    { "119",	 762000 },
    { "120",	 768000 },
    { "121",	 774000 },
    { "122",	 780000 },
    { "123",	 786000 },
    { "124",	 792000 },
    { "125",	 798000 },
 
    { "T7",	   7000 },  
    { "T8",	  13000 }, 
    { "T9",	  19000 }, 
    { "T10",	  25000 }, 
    { "T11",	  31000 }, 
    { "T12",	  37000 }, 
    { "T13",	  43000 }, 
    { "T14",	  49000 }, 
};

/* --------------------------------------------------------------------- */

/* JP broadcast */
static tveng_channel ntsc_bcast_jp[] = {
    { "1",   91250 },
    { "2",   97250 },
    { "3",  103250 },
    { "4",  171250 },
    { "5",  177250 },
    { "6",  183250 },
    { "7",  189250 },
    { "8",  193250 },
    { "9",  199250 },
    { "10", 205250 },
    { "11", 211250 },
    { "12", 217250 },

    { "13", 471250 },
    { "14", 477250 },
    { "15", 483250 },
    { "16", 489250 },
    { "17", 495250 },
    { "18", 501250 },
    { "19", 507250 },
    { "20", 513250 },
    { "21", 519250 },
    { "22", 525250 },
    { "23", 531250 },
    { "24", 537250 },
    { "25", 543250 },
    { "26", 549250 },
    { "27", 555250 },
    { "28", 561250 },
    { "29", 567250 },
    { "30", 573250 },
    { "31", 579250 },
    { "32", 585250 },
    { "33", 591250 },
    { "34", 597250 },
    { "35", 603250 },
    { "36", 609250 },
    { "37", 615250 },
    { "38", 621250 },
    { "39", 627250 },
    { "40", 633250 },
    { "41", 639250 },
    { "42", 645250 },
    { "43", 651250 },
    { "44", 657250 },
    { "45", 663250 },
    { "46", 669250 },
    { "47", 675250 },
    { "48", 681250 },
    { "49", 687250 },
    { "50", 693250 },
    { "51", 699250 },
    { "52", 705250 },
    { "53", 711250 },
    { "54", 717250 },
    { "55", 723250 },
    { "56", 729250 },
    { "57", 735250 },
    { "58", 741250 },
    { "59", 747250 },
    { "60", 753250 },
    { "61", 759250 },
    { "62", 765250 },
};

/* JP cable */
static tveng_channel ntsc_cable_jp[] = {
    { "13",	109250 },
    { "14",	115250 },
    { "15",	121250 },
    { "16",	127250 },
    { "17",	133250 },
    { "18",	139250 },
    { "19",	145250 },
    { "20",	151250 },
    { "21",	157250 },
    { "22",	165250 },
    { "23",	223250 },
    { "24",	231250 },
    { "25",	237250 },
    { "26",	243250 },
    { "27",	249250 },
    { "28",	253250 },
    { "29",	259250 },
    { "30",	265250 },
    { "31",	271250 },
    { "32",	277250 },
    { "33",	283250 },
    { "34",	289250 },
    { "35",	295250 },
    { "36",	301250 },
    { "37",	307250 },
    { "38",	313250 },
    { "39",	319250 },
    { "40",	325250 },
    { "41",	331250 },
    { "42",	337250 },
    { "43",	343250 },
    { "44",	349250 },
    { "45", 	355250 },
    { "46", 	361250 },
    { "47", 	367250 },
    { "48", 	373250 },
    { "49", 	379250 },
    { "50", 	385250 },
    { "51", 	391250 },
    { "52", 	397250 },
    { "53", 	403250 },
    { "54", 	409250 },
    { "55", 	415250 },
    { "56", 	421250 },
    { "57", 	427250 },
    { "58", 	433250 },
    { "59", 	439250 },
    { "60", 	445250 },
    { "61", 	451250 },
    { "62", 	457250 },
    { "63",	463250 },
};

/* --------------------------------------------------------------------- */

/* australia */
static tveng_channel pal_australia[] = {
    { "0",	 46250 },
    { "1",	 57250 },
    { "2",	 64250 },
    { "3",	 86250 },
    { "4",  	 95250 },
    { "5",  	102250 },
    { "6",  	175250 },
    { "7",  	182250 },
    { "8",  	189250 },
    { "9",  	196250 },
    { "10", 	209250 },
    { "11",	216250 },
    { "28",	527250 },
    { "29",	534250 },
    { "30",	541250 },
    { "31",	548250 },
    { "32",	555250 },
    { "33",	562250 },
    { "34",	569250 },
    { "35",	576250 },
    { "36",     591250 },
    { "39",	604250 },
    { "40",	611250 },
    { "41",	618250 },
    { "42",	625250 },
    { "43",	632250 },
    { "44",	639250 },
    { "45",	646250 },
    { "46",	653250 },
    { "47",	660250 },
    { "48",	667250 },
    { "49",	674250 },
    { "50",	681250 },
    { "51",	688250 },
    { "52",	695250 },
    { "53",	702250 },
    { "54",	709250 },
    { "55",	716250 },
    { "56",	723250 },
    { "57",	730250 },
    { "58",	737250 },
    { "59",	744250 },
    { "60",	751250 },
    { "61",	758250 },
    { "62",	765250 },
    { "63",	772250 },
    { "64",	779250 },
    { "65",	786250 },
    { "66",	793250 },
    { "67",	800250 },
    { "68",	807250 },
    { "69",	814250 },
};

/* --------------------------------------------------------------------- */
/* europe                                                                */

/* CCIR frequencies */

#define FREQ_CCIR_I_III		\
    { "E2",	  48250 },	\
    { "E3",	  55250 },	\
    { "E4",	  62250 },	\
				\
    { "S01",	  69250 },	\
    { "S02",	  76250 },	\
    { "S03",	  83250 },	\
				\
    { "E5",	 175250 },	\
    { "E6",	 182250 },	\
    { "E7",	 189250 },	\
    { "E8",	 196250 },	\
    { "E9",	 203250 },	\
    { "E10",	 210250 },	\
    { "E11",	 217250 },	\
    { "E12",	 224250 }

#define FREQ_CCIR_SL_SH		\
    { "SE1",	 105250 },	\
    { "SE2",	 112250 },	\
    { "SE3",	 119250 },	\
    { "SE4",	 126250 },	\
    { "SE5",	 133250 },	\
    { "SE6",	 140250 },	\
    { "SE7",	 147250 },	\
    { "SE8",	 154250 },	\
    { "SE9",	 161250 },	\
    { "SE10",    168250 },	\
				\
    { "SE11",    231250 },	\
    { "SE12",    238250 },	\
    { "SE13",    245250 },	\
    { "SE14",    252250 },	\
    { "SE15",    259250 },	\
    { "SE16",    266250 },	\
    { "SE17",    273250 },	\
    { "SE18",    280250 },	\
    { "SE19",    287250 },	\
    { "SE20",    294250 }

#define FREQ_CCIR_H	\
    { "S21", 303250 },	\
    { "S22", 311250 },	\
    { "S23", 319250 },	\
    { "S24", 327250 },	\
    { "S25", 335250 },	\
    { "S26", 343250 },	\
    { "S27", 351250 },	\
    { "S28", 359250 },	\
    { "S29", 367250 },	\
    { "S30", 375250 },	\
    { "S31", 383250 },	\
    { "S32", 391250 },	\
    { "S33", 399250 },	\
    { "S34", 407250 },	\
    { "S35", 415250 },	\
    { "S36", 423250 },	\
    { "S37", 431250 },	\
    { "S38", 439250 },	\
    { "S39", 447250 },	\
    { "S40", 455250 },	\
    { "S41", 463250 }

/* OIRT frequencies */

#define FREQ_OIRT_I_III		\
    { "R1",       49750 },	\
    { "R2",       59250 },	\
				\
    { "R3",       77250 },	\
    { "R4",       84250 },	\
    { "R5",       93250 },	\
				\
    { "R6",	 175250 },	\
    { "R7",	 183250 },	\
    { "R8",	 191250 },	\
    { "R9",	 199250 },	\
    { "R10",	 207250 },	\
    { "R11",	 215250 },	\
    { "R12",	 223250 }

#define FREQ_OIRT_SL_SH		\
    { "SR1",	 111250 },	\
    { "SR2",	 119250 },	\
    { "SR3",	 127250 },	\
    { "SR4",	 135250 },	\
    { "SR5",	 143250 },	\
    { "SR6",	 151250 },	\
    { "SR7",	 159250 },	\
    { "SR8",	 167250 },	\
				\
    { "SR11",    231250 },	\
    { "SR12",    239250 },	\
    { "SR13",    247250 },	\
    { "SR14",    255250 },	\
    { "SR15",    263250 },	\
    { "SR16",    271250 },	\
    { "SR17",    279250 },	\
    { "SR18",    287250 },	\
    { "SR19",    295250 }

#define FREQ_UHF	\
    { "21",  471250 },	\
    { "22",  479250 },	\
    { "23",  487250 },	\
    { "24",  495250 },	\
    { "25",  503250 },	\
    { "26",  511250 },	\
    { "27",  519250 },	\
    { "28",  527250 },	\
    { "29",  535250 },	\
    { "30",  543250 },	\
    { "31",  551250 },	\
    { "32",  559250 },	\
    { "33",  567250 },	\
    { "34",  575250 },	\
    { "35",  583250 },	\
    { "36",  591250 },	\
    { "37",  599250 },	\
    { "38",  607250 },	\
    { "39",  615250 },	\
    { "40",  623250 },	\
    { "41",  631250 },	\
    { "42",  639250 },	\
    { "43",  647250 },	\
    { "44",  655250 },	\
    { "45",  663250 },	\
    { "46",  671250 },	\
    { "47",  679250 },	\
    { "48",  687250 },	\
    { "49",  695250 },	\
    { "50",  703250 },	\
    { "51",  711250 },	\
    { "52",  719250 },	\
    { "53",  727250 },	\
    { "54",  735250 },	\
    { "55",  743250 },	\
    { "56",  751250 },	\
    { "57",  759250 },	\
    { "58",  767250 },	\
    { "59",  775250 },	\
    { "60",  783250 },	\
    { "61",  791250 },	\
    { "62",  799250 },	\
    { "63",  807250 },	\
    { "64",  815250 },	\
    { "65",  823250 },	\
    { "66",  831250 },	\
    { "67",  839250 },	\
    { "68",  847250 },	\
    { "69",  855250 }

static tveng_channel pal_europe[] = {
    FREQ_CCIR_I_III,
    FREQ_CCIR_SL_SH,
    FREQ_CCIR_H,
    FREQ_UHF
};

static tveng_channel pal_europe_east[] = {
    FREQ_OIRT_I_III,
    FREQ_OIRT_SL_SH,
    FREQ_CCIR_H,
    FREQ_UHF
};

static tveng_channel pal_italy[] = {
    { "2",	 53750 },
    { "3",	 62250 },
    { "4",	 82250 },
    { "5",	175250 },
    { "6",	183750 },
    { "7",	192250 },
    { "8",	201250 },
    { "9",	210250 },
    { "10",	210250 },
    { "11",	217250 },
    { "12",	224250 },
};

static tveng_channel pal_ireland[] = {
    { "0",    45750 },
    { "1",    53750 },
    { "2",    61750 },
    { "3",   175250 },
    { "4",   183250 },
    { "5",   191250 },
    { "6",   199250 },
    { "7",   207250 },
    { "8",   215250 },
    FREQ_UHF,
};

static tveng_channel secam_france[] = {
    { "K01",    47750 },
    { "K02",    55750 },
    { "K03",    60500 },
    { "K04",    63750 },
    { "K05",   176000 },
    { "K06",   184000 },
    { "K07",   192000 },
    { "K08",   200000 },
    { "K09",   208000 },
    { "K10",   216000 },
    { "KB",    116750 },
    { "KC",    128750 },
    { "KD",    140750 },
    { "KE",    159750 },
    { "KF",    164750 },
    { "KG",    176750 },
    { "KH",    188750 },
    { "KI",    200750 },
    { "KJ",    212750 },
    { "KK",    224750 },
    { "KL",    236750 },
    { "KM",    248750 },
    { "KN",    260750 },
    { "KO",    272750 },
    { "KP",    284750 },
    { "KQ",    296750 },
    { "H01",   303250 },
    { "H02",   311250 },
    { "H03",   319250 },
    { "H04",   327250 },
    { "H05",   335250 },
    { "H06",   343250 },
    { "H07",   351250 },
    { "H08",   359250 },
    { "H09",   367250 },
    { "H10",   375250 },
    { "H11",   383250 },
    { "H12",   391250 },
    { "H13",   399250 },
    { "H14",   407250 },
    { "H15",   415250 },
    { "H16",   423250 },
    { "H17",   431250 },
    { "H18",   439250 },
    { "H19",   447250 },
    FREQ_UHF,
};

/* --------------------------------------------------------------------- */

static tveng_channel pal_newzealand[] = {
    { "1", 	  45250 }, 
    { "2",	  55250 }, 
    { "3",	  62250 },
    { "4",	 175250 },
    { "5",	 182250 },
    { "5A",	 138250 },
    { "6",	 189250 },
    { "7",	 196250 },
    { "8",	 203250 },
    { "9",	 210250 },
    { "10",	 217250 },
};

static tveng_channel secam_russia[] = {
    { "1", 	  48500 }, 
    { "2", 	  58000 }, 
    { "3", 	  76000 }, 
    { "4", 	  84000 }, 
    { "5", 	  92000 }, 
    { "6", 	 174000 }, 
    { "7", 	 182000 }, 
    { "8", 	 190000 }, 
    { "9", 	 198000 }, 
    { "10", 	 206000 }, 
    { "11", 	 214000 }, 
    { "12", 	 222000 }, 
    { "21", 	 470000 }, 
    { "22", 	 478000 }, 
    { "23", 	 486000 }, 
    { "24", 	 494000 }, 
    { "25", 	 502000 }, 
    { "26", 	 510000 }, 
    { "27", 	 518000 }, 
    { "28", 	 526000 }, 
    { "29", 	 534000 }, 
    { "30", 	 542000 }, 
    { "31", 	 550000 }, 
    { "32", 	 558000 }, 
    { "33", 	 566000 }, 
    { "34", 	 574000 }, 
    { "35", 	 582000 }, 
    { "36", 	 590000 }, 
    { "37", 	 598000 }, 
    { "38", 	 606000 }, 
    { "39", 	 614000 }, 
    { "40", 	 622000 }, 
    { "41", 	 630000 }, 
    { "42", 	 638000 }, 
    { "43", 	 646000 }, 
    { "44", 	 654000 }, 
    { "45", 	 662000 }, 
    { "46", 	 670000 }, 
    { "47", 	 678000 }, 
    { "48", 	 686000 }, 
    { "49", 	 694000 }, 
    { "50", 	 702000 }, 
    { "51", 	 710000 }, 
    { "52", 	 718000 }, 
    { "53", 	 726000 }, 
    { "54", 	 734000 }, 
    { "55", 	 742000 }, 
    { "56", 	 750000 }, 
    { "57", 	 758000 }, 
    { "58", 	 766000 }, 
    { "59", 	 774000 }, 
    { "60", 	 782000 }, 
};

static tveng_channel south_africa[] = {
  { "4",	175250 },
  { "5",	183250 },
  { "6",	191250 },
  { "7",	199250 },
  { "8",	207250 },
  { "9",	215250 },
  { "10",	223250 },
  { "11",	231250 },
  { "13",	247430 },
  { "21",	471250 },
  { "22",	479250 },
  { "23",	487250 },
  { "24",	495250 },
  { "25",	503250 },
  { "26",	511250 },
  { "27",	519250 },
  { "28",	527250 },
  { "29",	535250 },
  { "30",	543250 },
  { "31",	551250 },
  { "32",	559250 },
  { "33",	567250 },
  { "34",	575250 },
  { "35",	583250 },
  { "36",	591250 },
  { "37",	599250 },
  { "38",	607250 },
  { "39",	615250 },
  { "40",	623250 },
  { "41",	631250 },
  { "42",	639250 },
  { "43",	647250 },
  { "44",	655250 },
  { "45",	663250 },
  { "46",	671250 },
  { "47",	679250 },
  { "48",	687250 },
  { "49",	695250 },
  { "50",	703250 },
  { "51",	711250 },
  { "52",	719250 },
  { "53",	727250 },
  { "54",	735250 },
  { "55",	743250 },
  { "56",	751250 },
  { "57",	759250 },
  { "58",	767250 },
  { "59",	775250 },
  { "60",	783250 },
  { "61",	791250 },
  { "62",	799250 },
  { "63",	807250 },
  { "64",	815250 },
  { "65",	823250 },
  { "66",	831250 },
  { "67",	839250 },
  { "68",	847250 }
};

static tveng_channel pal_bcast_cn[] = {
    { "1",	49750 },
    { "2",	57750 },
    { "3",	65750 },
    { "4",	77250 },
    { "5",	85250 },
    { "6",	112250 },
    { "7",	120250 },
    { "8",	128250 },
    { "9",	136250 },
    { "10",	144250 },
    { "11",	152250 },
    { "12",	160250 },
    { "13",	168250 },
    { "14",	176250 },
    { "15",	184250 },
    { "16",	192250 },
    { "17",	200250 },
    { "18",	208250 },
    { "19",	216250 },
    { "20",	224250 },
    { "21",	232250 },
    { "22",	240250 },
    { "23",	248250 },
    { "24",	256250 },
    { "25",	264250 },
    { "26",	272250 },
    { "27",	280250 },
    { "28",	288250 },
    { "29",	296250 },
    { "30",	304250 },
    { "31",	312250 },
    { "32",	320250 },
    { "33",	328250 },
    { "34",	336250 },
    { "35",	344250 },
    { "36",	352250 },
    { "37",	360250 },
    { "38",	368250 },
    { "39",	376250 },
    { "40",	384250 },
    { "41",	392250 },
    { "42",	400250 },
    { "43",	408250 },
    { "44",	416250 },
    { "45",	424250 },
    { "46",	432250 },
    { "47",	440250 },
    { "48",	448250 },
    { "49",	456250 },
    { "50",	463250 },
    { "51",	471250 },
    { "52",	479250 },
    { "53",	487250 },
    { "54",	495250 },
    { "55",	503250 },
    { "56",	511250 },
    { "57",	519250 },
    { "58",	527250 },
    { "59",	535250 },
    { "60",	543250 },
    { "61",	551250 },
    { "62",	559250 },
    { "63",	607250 },
    { "64",	615250 },
    { "65",	623250 },
    { "66",	631250 },
    { "67",	639250 },
    { "68",	647250 },
    { "69",	655250 },
    { "70",	663250 },
    { "71",	671250 },
    { "72",	679250 },
    { "73",	687250 },
    { "74",	695250 },
    { "75",	703250 },
    { "76",	711250 },
    { "77",	719250 },
    { "78",	727250 },
    { "79",	735250 },
    { "80",	743250 },
    { "81",	751250 },
    { "82",	759250 },
    { "83",	767250 },
    { "84",	775250 },
    { "85",	783250 },
    { "86",	791250 },
    { "87",	799250 },
    { "88",	807250 },
    { "89",	815250 },
    { "90",	823250 },
    { "91",	831250 },
    { "92",	839250 },
    { "93",	847250 },
    { "94",	855250 },
};

/* --------------------------------------------------------------------- */

static tveng_channels chanlists[] = {
    { N_("us-bcast"),      ntsc_bcast,      CHAN_COUNT(ntsc_bcast)      },
    { N_("us-cable"),      ntsc_cable,      CHAN_COUNT(ntsc_cable)      },
    { N_("us-cable-hrc"),  ntsc_hrc,        CHAN_COUNT(ntsc_hrc)        },
    { N_("japan-bcast"),   ntsc_bcast_jp,   CHAN_COUNT(ntsc_bcast_jp)   },
    { N_("japan-cable"),   ntsc_cable_jp,   CHAN_COUNT(ntsc_cable_jp)   },
    { N_("europe"),        pal_europe,      CHAN_COUNT(pal_europe)      },
    { N_("europe-east"),   pal_europe_east, CHAN_COUNT(pal_europe_east) },
    { N_("italy"),         pal_italy,       CHAN_COUNT(pal_italy)       },
    { N_("newzealand"),    pal_newzealand,  CHAN_COUNT(pal_newzealand)  },
    { N_("australia"),     pal_australia,   CHAN_COUNT(pal_australia)   },
    { N_("ireland"),       pal_ireland,     CHAN_COUNT(pal_ireland)     },
    { N_("france"),        secam_france,    CHAN_COUNT(secam_france)    },
    { N_("russia"),        secam_russia,    CHAN_COUNT(secam_russia)    },
    { N_("south africa"),  south_africa,    CHAN_COUNT(south_africa)    },
    { N_("china"),	   pal_bcast_cn,    CHAN_COUNT(pal_bcast_cn)    },
    { NULL, NULL, 0 } /* EOF */
};

/* 
   Returns a pointer to the channel struct for some specific
   country. NULL if the specified country is not found.
   The country should be in english.
*/
tveng_channels*
tveng_get_country_tune_by_name (gchar * country)
{
  int i=0;

  if (country == NULL)
    return NULL;
  
  while (chanlists[i].name)
    {
      if (!strcasecmp(chanlists[i].name, country))
	return (&(chanlists[i]));
      i++;
    }

  return NULL;
}

/* 
   Returns a pointer to the channel struct for some specific
   country. NULL if the specified country is not found.
   The given name can be i18ed this time.
*/
tveng_channels*
tveng_get_country_tune_by_i18ed_name (gchar * i18ed_country)
{
  int i=0;

  if (i18ed_country == NULL)
    return NULL;
  
  while (chanlists[i].name)
    {
      if (!strcasecmp(_(chanlists[i].name), i18ed_country))
	return (&(chanlists[i]));
      i++;
    }

  return NULL;
}

/*
  Returns a pointer to the specified by id channel. Returns NULL on
  error.
  This is useful if you want to get all the countries we know about,
  you can start from id 0, and go up until you get an error.
*/
tveng_channels*
tveng_get_country_tune_by_id (int id)
{
  int num_countries = sizeof(chanlists)/sizeof(tveng_channels);
  num_countries --; /* NULL entry in the end */

  if (id >= num_countries)
    return NULL;
  if (id < 0)
    return NULL;

  return &(chanlists[id]);
}

/*
  Returns the id of the given country tune, that could be used later
  on with tveng_get_country_tune_by_id. Returns -1 on error.
*/
int
tveng_get_id_of_country_tune (tveng_channels * country)
{
  int i=0;

  if (country == NULL)
    return -1;
  
  while (chanlists[i].name)
    {
      if (!strcasecmp(chanlists[i].name, country->name))
	return i; /* Found */
      i++;
    }

  return -1; /* Nothing found */
}

/*
  Finds an especific channel in an especific country by name. NULL on
  error.
*/
tveng_channel*
tveng_get_channel_by_name (gchar* name, tveng_channels * country)
{
  int i;

  if (name == NULL)
    return NULL;

  if (country == NULL)
    return NULL;

  for (i=0; i<country->chan_count; i++)
    if (!strcasecmp(name, country->channel_list[i].name))
      return &(country->channel_list[i]);

  return NULL;
}

/*
  Finds an especific channel in an especific country by its id. NULL on
  error.
*/
tveng_channel*
tveng_get_channel_by_id (int id, tveng_channels * country)
{
  if (id < 0)
    return NULL;
  if (country == NULL)
    return NULL;
  if (id >= country->chan_count)
    return NULL;
  return &(country->channel_list[id]);
}

/*
  Returns the id of the given channel, that can be used later with
  tveng_get_channel_by_id. Returns -1 on error.
*/
int
tveng_get_id_of_channel (tveng_channel * channel, tveng_channels *
			 country)
{
  int i;

  if (channel == NULL)
    return -1;

  if (country == NULL)
    return -1;

  for (i=0; i<country->chan_count; i++)
    if ((!strcasecmp(channel->name, country->channel_list[i].name)) && 
	(channel -> freq == country->channel_list[i].freq))
      return i; /* Found */

  return -1; /* Nothing found */
}

/* Tuned channels API */
/*
  Maybe it looks too much code for very little thing, but this way we
  simplify the rest of the code and it is easier to change
*/

/**
 * Returns the first item in the channel list, or NULL
 */
static tveng_tuned_channel *
first_channel(tveng_tuned_channel * list)
{
  if (!list)
    return NULL;

  while (list->prev)
    list = list->prev;

  return list;    
}


/*
  This function inserts a channel in the list (the list will keep
  alphabetically ordered).
  It returns the index where the channel is inserted.
*/
tveng_tuned_channel *
tveng_insert_tuned_channel (tveng_tuned_channel * new_channel,
			    tveng_tuned_channel * list)
{
  tveng_tuned_channel * channel_added = (tveng_tuned_channel*)
    g_malloc0(sizeof(tveng_tuned_channel));
  tveng_tuned_channel * tc_ptr = first_channel(list);
  int index = 0; /* Where are we storing it */

  if (!new_channel)
    return list;

  list = first_channel(list);

  if (new_channel->name)
    channel_added->name = g_strdup(new_channel->name);
  else
    channel_added->name = g_strdup(_("(Unnamed channel)"));
  if (new_channel->real_name)
    channel_added->real_name = g_strdup(new_channel->real_name);
  else
    channel_added->real_name = g_strdup(_("(Unknown real channel)"));
  if (new_channel->country)
    channel_added->country = g_strdup(new_channel->country);
  else
    channel_added->country = g_strdup(_("(Unknown country)"));

  channel_added->accel_key = new_channel->accel_key;
  channel_added->accel_mask = new_channel->accel_mask;
  channel_added->freq = new_channel->freq;
  channel_added->input = new_channel->input;
  channel_added->standard = new_channel->standard;

  /* OK, we are starting the list */
  if (!tc_ptr)
    {
      channel_added->index = 0;
      channel_added->next = channel_added->prev = NULL;
      return channel_added;
    }

  /* The list is already started, proceed until we reach the desired
     channel */
#ifndef DISABLE_CHANNEL_ORDERING /* ordering can be disabled */
  while (tc_ptr)
    {
      /* If this one orders itself after us, then insert it here */
      if (strnatcasecmp(tc_ptr -> name, channel_added -> name) >=
	  0)
	{
	  /* If two are the same string, but one has capitals and the
	     other one doesn't, insert first the capitalized one */
	  if ((strnatcasecmp(tc_ptr -> name, channel_added -> name) == 0)
	       && (strnatcmp(tc_ptr -> name, channel_added -> name) < 0) )
	      { /* Insert better in the next position */
		index++;
		if (!tc_ptr -> next) /* Insert it as the last item */
		  break; /* End the loop */

		tc_ptr = tc_ptr -> next;
		continue;
	      }

	  /* replace tc_ptr with the current one */
	  channel_added -> prev = tc_ptr -> prev;

	  if (tc_ptr == list)
	    list = channel_added; /* We are adding the first
				     item */
      
	  if (tc_ptr -> prev)
	    tc_ptr -> prev -> next = channel_added;
      
	  channel_added -> next = tc_ptr;
	  tc_ptr -> prev = channel_added;
	  channel_added -> index = index;

	  while (tc_ptr) /* Update indexes */
	    {
	      tc_ptr->index++;
	      tc_ptr = tc_ptr->next;
	    }

	  return first_channel(list);
	}

      index++;
      if (!tc_ptr -> next)
	break; /* Exit the loop to add this entry as the last one */

      tc_ptr = tc_ptr -> next;
    }
#else /* no ordering, just go to the last entry */
  while (tc_ptr)
    {
      index++;
      if (!tc_ptr -> next)
	break; /* last_one reached */
      tc_ptr = tc_ptr -> next;
    }
#endif

  /* Add this entry as the last one */
  tc_ptr -> next = channel_added;
  channel_added -> prev = tc_ptr;
  channel_added -> index = index;
  channel_added -> next = NULL;

  return first_channel(list);
}

/*
  Returns the number of items in the list
*/
int
tveng_tuned_channel_num (tveng_tuned_channel * list)
{
  int num_channels = 0;
  tveng_tuned_channel * tc_ptr = first_channel(list);

  while (tc_ptr)
    {
      num_channels++;
      tc_ptr = tc_ptr -> next;
    }

  return (num_channels);
}

/*
  Removes an specific channel form the list. You must provide its
  "real" name, i.e. "64" instead of "Tele5", for example. Returns -1
  if the channel could not be found. If real_name is NULL, then id is
  interpreted as the index in the tuned_channel list. Then -1 means
  out of bounds. if real_name is not NULL, then the first matching
  item from id is deleted.
*/
tveng_tuned_channel *
tveng_remove_tuned_channel (gchar * real_name, int id,
			    tveng_tuned_channel * list)
{
  tveng_tuned_channel * tc_ptr;
  tveng_tuned_channel * tc_ptr_traverse;

  list = first_channel(list);

  if (!list)
    return NULL;
  
  /* We don't have a real name, use the id as the offset */
  if (!real_name)
    {
      tc_ptr = tveng_retrieve_tuned_channel_by_index(id, list);
      if (!tc_ptr)
	return list;
    }
  else /* We have a real name, go find it */
    {
      tc_ptr = tveng_retrieve_tuned_channel_by_real_name(real_name,
							id, list);
      if (!tc_ptr)
	return list;
    }

  /* We have the desired channel, delete it */
  if (tc_ptr -> prev)
    tc_ptr -> prev -> next = tc_ptr -> next;
  if (tc_ptr -> next)
    tc_ptr -> next -> prev = tc_ptr -> prev;

  g_free(tc_ptr -> name);
  g_free(tc_ptr -> real_name);
  g_free(tc_ptr -> country);

  if (list == tc_ptr) /* We are deleting the first item */
    list = tc_ptr -> next;

  /* Update indexes */
  tc_ptr_traverse = tc_ptr -> next;
  while (tc_ptr_traverse)
    {
      tc_ptr_traverse -> index--;
      tc_ptr_traverse = tc_ptr_traverse -> next;
    }
  
  /* We are done, kill this channel */
  g_free(tc_ptr);

  return list;
}

/*
  Copies src into dest
*/
void
tveng_copy_tuned_channel(tveng_tuned_channel * dest,
			 tveng_tuned_channel * src)
{
  g_free(dest->name);
  g_free(dest->real_name);
  g_free(dest->country);

  if (src->name)
    dest->name = g_strdup(src->name);
  else
    dest->name = g_strdup(_("(Unnamed channel)"));
  if (src->real_name)
    dest->real_name = g_strdup(src->real_name);
  else
    dest->real_name = g_strdup(_("(Unknown real channel)"));
  if (src->country)
    dest->country = g_strdup(src->country);
  else
    dest->country = g_strdup(_("(Unknown country)"));

  dest->input = src->input;
  dest->standard = src->standard;

  dest->accel_key = src->accel_key;
  dest->accel_mask = src->accel_mask;
  dest->freq = src->freq;
}

/*
  Removes all the items in the channel list
*/
tveng_tuned_channel *
tveng_clear_tuned_channel (tveng_tuned_channel * list)
{
  while ((list = tveng_remove_tuned_channel(NULL, 0, list)));

  return list;
}

/*
  Retrieves the specified channel form the list, searching by name
  ("VOX"), and starting from index. Returns NULL on error. It uses
  strcasecomp(), so "VoX" matches "vOx", "Vox", "voX", ...
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_name (gchar * name, int index,
				      tveng_tuned_channel * list)
{
  tveng_tuned_channel * tc_ptr =
    tveng_retrieve_tuned_channel_by_index (index, list);

  if (!tc_ptr)
    return NULL;

  if (!name)
    return NULL;

  while (tc_ptr)
    {
      if (!strcasecmp(name, tc_ptr -> name))
	return tc_ptr;

      tc_ptr = tc_ptr -> next;
    }

  return NULL; /* If we get here nothing has been found */
}

/*
  Retrieves the specified channel by real name ("S23"), and starting
  from index. Returns NULL on error. Again a strcasecmp
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_real_name (gchar * real_name, int index,
					   tveng_tuned_channel * list)
{
  tveng_tuned_channel * tc_ptr =
    tveng_retrieve_tuned_channel_by_index (index, list);

  if (!tc_ptr)
    return NULL;

  if (!real_name)
    return NULL;

  while (tc_ptr)
    {
      if (!strcasecmp(real_name, tc_ptr -> real_name))
	return tc_ptr;

      tc_ptr = tc_ptr -> next;
    }

  return NULL; /* If we get here nothing has been found */
}

/*
  Retrieves the channel in position "index". NULL on error
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_index (int index,
				       tveng_tuned_channel * list)
{
  tveng_tuned_channel * tc_ptr = first_channel(list);

  g_assert(index >= 0);

  if (index >= tveng_tuned_channel_num(list))
    return NULL; /* probably just traversing */

  while (tc_ptr)
    {
      if (tc_ptr -> index == index)
	return tc_ptr;
      tc_ptr = tc_ptr -> next;
    }

  g_assert_not_reached(); /* This shouldn't be reached if everything
			     works */
  return NULL;
}
