/* $Id: network-table.h,v 1.7.2.2 2007-11-01 00:21:24 mschimek Exp $ */

/* Generated file, do not edit! */

struct country {
	uint16_t		cni_8301;	/* Packet 8/30 format 1 */
	uint16_t		cni_8302;	/* Packet 8/30 format 2 */
	uint16_t		cni_pdc_b;	/* Packet X/26 (PDC B) */
	uint16_t		cni_vps;	/* VPS */
	const char		country_code[4];
	const char *		name;		/* UTF-8 */
};

struct network {
	uint16_t		cni_8301;	/* Packet 8/30 format 1 */
	uint16_t		cni_8302;	/* Packet 8/30 format 2 */
	uint16_t		cni_pdc_b;	/* Packet X/26 (PDC B) */
	uint16_t		cni_vps;	/* VPS */
	unsigned int		country;
	const char *		name;		/* UTF-8 */
	const struct _vbi3_network_pdc *pdc;
};

struct ttx_header {
	const char *		name;		/* UTF-8 */
	const uint8_t *		header;		/* raw Teletext data */
};

static const struct country
country_table [] = {
	{ 0x0000, 0x0000, 0x0000, 0x0000, "AA", "Unknown" },
	{ 0x4300, 0x0000, 0x0000, 0x0A00, "AT", "Austria" },
	{ 0x3200, 0x1600, 0x0000, 0x0000, "BE", "Belgium" },
	{ 0x4100, 0x2400, 0x3400, 0x0400, "CH", "Switzerland" },
	{ 0x0000, 0x3200, 0x0000, 0x0000, "CZ", "Czech Republic" },
	{ 0x4900, 0x0000, 0x3D00, 0x0D00, "DE", "Germany" },
	{ 0x0000, 0x2900, 0x0000, 0x0000, "DK", "Denmark" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "ES", "Spain" },
	{ 0x3500, 0x2600, 0x0000, 0x0000, "FI", "Finland" },
	{ 0x3300, 0x2F00, 0x3F00, 0x0000, "FR", "France" },
	{ 0x4400, 0x2C00, 0x3C00, 0x0000, "GB", "United Kingdom" },
	{ 0x3000, 0x2100, 0x3100, 0x0000, "GR", "Greece" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "HR", "Croatia" },
	{ 0x3600, 0x0000, 0x0000, 0x0000, "HU", "Hungary" },
	{ 0x0000, 0x4200, 0x3200, 0x0000, "IE", "Ireland" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "IS", "Iceland" },
	{ 0x3900, 0x1500, 0x0000, 0x0000, "IT", "Italy" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "LU", "Luxembourg" },
	{ 0x3100, 0x4800, 0x3800, 0x0000, "NL", "Netherlands" },
	{ 0x4700, 0x0000, 0x0000, 0x0000, "NO", "Norway" },
	{ 0x4800, 0x0000, 0x0000, 0x0000, "PL", "Poland" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "PT", "Portugal" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "SE", "Sweden" },
	{ 0xAA00, 0x0000, 0x0000, 0x0000, "SI", "Slovenia" },
	{ 0x0000, 0x3500, 0x3500, 0x0000, "SK", "Slovakia" },
	{ 0x0000, 0x0000, 0x0000, 0x0000, "SM", "San Marino" },
	{ 0x9000, 0x0000, 0x0000, 0x0000, "TR", "Turkey" },
	{ 0x7700, 0x0000, 0x0000, 0x0700, "UA", "Ukraine" },
};

static const struct _vbi3_network_pdc
pdc_table_72 [] = {
	{ 0x301, 0x304, " %d.%m.%y ", 0, "%H.%M", 0, { NULL }, { 0 } },
	{ 0x305, 0x308, " %d.%m.%y ", 0, "%H.%M", 1440, { NULL }, { 0 } },
	{ 0x309, 0x309, " %d.%m.%y ", 0, "%H.%M", 2880, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_73 [] = {
	{ 0x301, 0x304, " %d.%m ", 0, "%H.%M", 0, { NULL }, { 0 } },
	{ 0x305, 0x308, " %d.%m ", 0, "%H.%M", 1440, { NULL }, { 0 } },
	{ 0x309, 0x309, " %d.%m ", 0, "%H.%M", 2880, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_97 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_111 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_69 [] = {
	{ 0x301, 0x304, " %d.%m.%y ", 0, "%k.%M", 0, { NULL }, { 0 } },
	{ 0x305, 0x306, " %d.%m.%y ", 0, "%k.%M", 1440, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_193 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_205 [] = {
	{ 0x301, 0x304, " %d.%m. ", 0, "%H.%M", 0, { NULL }, { 0 } },
	{ 0x305, 0x308, " %d.%m. ", 0, "%H.%M", 1440, { NULL }, { 0 } },
	{ 0x309, 0x309, " %d.%m. ", 0, "%H.%M", 2880, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_117 [] = {
	{ 0x301, 0x308, " %d.%m.%y ", 2, NULL, 0, { NULL }, { 0 } },
	{ 0x351, 0x357, " %d.%m.%y ", 2, NULL, 0, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_118 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_60 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_68 [] = {
	{ 0x301, 0x304, " %d.%m. ", 4, "%H:%M", 0, { NULL }, { 0 } },
	{ 0x305, 0x308, " %d.%m. ", 4, "%H:%M", 0, { NULL }, { 0 } },
	{ 0x309, 0x309, " %d.%m. ", 4, "%H:%M", 0, { NULL }, { 0 } },
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_122 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_126 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_148 [] = {
	{ 0 }
};

static const struct _vbi3_network_pdc
pdc_table_177 [] = {
	{ 0 }
};

static const struct network
network_table [] = {
	{ 0x0000, 0x0000, 0x0000, 0x0481,  3, "TeleZüri", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0482,  3, "Teleclub Abo-Fernsehen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0483,  3, "Zürich 1", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0484,  3, "TeleBern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0485,  3, "Tele M1", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0486,  3, "Star TV", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0487,  3, "Pro 7", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0488,  3, "TopTV", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0AC3,  1, "ORF 3", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0ACB,  1, "ORF Burgenland", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0ACC,  1, "ORF Kärnten", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0ACD,  1, "ORF Niederösterreich", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0ACE,  1, "ORF Oberösterreich", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0ACF,  1, "ORF Salzburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0AD0,  1, "ORF Steiermark", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0AD1,  1, "ORF Tirol", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0AD2,  1, "ORF Vorarlberg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0AD3,  1, "ORF Wien", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D75,  5, "KDG Infokanal", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D76,  5, "DAS VIERTE", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D79,  5, "RTL Shop", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D7A,  5, "N24", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D7B,  5, "TV Berlin", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D7C,  5, "ONYX-TV", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D7E,  5, "Nickelodeon", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D81,  5, "ORB-1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D87,  5, "1A-Fernsehen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D88,  5, "VIVA", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D89,  5, "VIVA 2", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D8A,  5, "Super RTL", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D8B,  5, "RTL Club", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D8C,  5, "n-tv", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D8D,  5, "DSF", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D90,  5, "RTL 2 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D91,  5, "Eurosport", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D92,  5, "Kabel 1", pdc_table_72 },
	{ 0x0000, 0x0000, 0x0000, 0x0D94,  5, "PRO 7", pdc_table_73 },
	{ 0x0000, 0x0000, 0x0000, 0x0D95,  5, "SAT 1 Brandenburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D96,  5, "SAT 1 Thüringen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D97,  5, "SAT 1 Sachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D98,  5, "SAT 1 Mecklenb.-Vorpommern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D99,  5, "SAT 1 Sachsen-Anhalt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9A,  5, "RTL Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9B,  5, "RTL Schleswig-Holstein", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9C,  5, "RTL Hamburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9D,  5, "RTL Berlin", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9E,  5, "RTL Niedersachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0D9F,  5, "RTL Bremen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA0,  5, "RTL Nordrhein-Westfalen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA1,  5, "RTL Hessen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA2,  5, "RTL Rheinland-Pfalz", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA3,  5, "RTL Baden-Württemberg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA4,  5, "RTL Bayern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA5,  5, "RTL Saarland", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA6,  5, "RTL Sachsen-Anhalt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA7,  5, "RTL Mecklenburg-Vorpommern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA8,  5, "RTL Sachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DA9,  5, "RTL Thüringen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DAA,  5, "RTL Brandenburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DAB,  5, "RTL", pdc_table_97 },
	{ 0x0000, 0x0000, 0x0000, 0x0DAC,  5, "Premiere", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DAD,  5, "SAT 1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DAE,  5, "SAT 1 Schleswig-Holstein", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DAF,  5, "SAT 1 Hamburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB0,  5, "SAT 1 Berlin", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB1,  5, "SAT 1 Niedersachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB2,  5, "SAT 1 Bremen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB3,  5, "SAT 1 Nordrhein-Westfalen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB4,  5, "SAT 1 Hessen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB5,  5, "SAT 1 Rheinland-Pfalz", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB6,  5, "SAT 1 Baden-Württemberg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB7,  5, "SAT 1 Bayern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB8,  5, "SAT 1 Saarland", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DB9,  5, "SAT 1", pdc_table_111 },
	{ 0x0000, 0x0000, 0x0000, 0x0DBA,  5, "NEUN LIVE", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DBB,  5, "Deutsche Welle TV Berlin", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DBD,  5, "Berlin Offener Kanal", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DBE,  5, "Berlin-Mix-Channel 2", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DBF,  5, "Berlin-Mix-Channel 1", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DC3,  5, "ARD/ZDF Vormittagsprogramm", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DC4,  5, "ARD-TV-Sternpunkt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DC5,  5, "ARD-TV-Sternpunkt-Fehler", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DCA,  5, "BR-1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DCC,  5, "BR-3 Süd", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DCD,  5, "BR-3 Nord", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DCE,  5, "HR-1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD0,  5, "NDR-1 Dreiländerweit", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD1,  5, "NDR-1 Hamburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD2,  5, "NDR-1 Niedersachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD3,  5, "NDR-1 Schleswig-Holstein", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD4,  5, "Nord-3 (NDR/SFB/RB)", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD6,  5, "NDR-3 Hamburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD7,  5, "NDR-3 Niedersachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD8,  5, "NDR-3 Schleswig-Holstein", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DD9,  5, "RB-1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DDB,  5, "SFB-1 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DDD,  5, "SWR-1 Baden-Württemberg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DDE,  5, "SWR-1 Rheinland-Pfalz", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE0,  5, "Südwest 3 (SDR/SR/SWF)", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE2,  5, "SW 3 Saarland", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE3,  5, "SW 3 Baden-Württemb. Süd", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE5,  5, "WDR-1 Regionalprogramm", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE7,  5, "WDR-3 Bielefeld", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE8,  5, "WDR-3 Dortmund", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DE9,  5, "WDR-3 Düsseldorf", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DEA,  5, "WDR-3 Köln", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DEB,  5, "WDR-3 Münster", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DEC,  5, "SW 3 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DED,  5, "SW 3 Baden-Württemberg Nord", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DEE,  5, "SW 3 Mannheim", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DEF,  5, "SW 3 Regional", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF0,  5, "SWR1 / Regionalprogramm", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF1,  5, "NDR-1 Mecklenb.-Vorpommern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF2,  5, "NDR-3 Mecklenb.-Vorpommern", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF3,  5, "MDR-1 Sachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF4,  5, "MDR-3 Sachsen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF5,  5, "MDR Dresden", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF6,  5, "MDR-1 Sachsen-Anhalt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF7,  5, "WDR Dortmund", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF8,  5, "MDR-3 Sachsen-Anhalt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DF9,  5, "MDR Magdeburg", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DFA,  5, "MDR-1 Thüringen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DFB,  5, "MDR-3 Thüringen", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DFC,  5, "MDR Erfurt", NULL },
	{ 0x0000, 0x0000, 0x0000, 0x0DFD,  5, "MDR-1 Regional", NULL },
	{ 0x01F2, 0x5BF1, 0x3B71, 0x0000, 10, "CNN International", NULL },
	{ 0x0385, 0x0000, 0x0000, 0x0000, 12, "HRT", NULL },
	{ 0x0404, 0x1604, 0x3604, 0x0000,  2, "VT4", NULL },
	{ 0x04F9, 0x0000, 0x0000, 0x0000,  0, "Zee TV", NULL },
	{ 0x0AE8, 0x0000, 0x0000, 0x0D14,  5, "PRO 7 Austria", NULL },
	{ 0x0D8F, 0x0000, 0x0000, 0x0D8F,  5, "RTL 2", pdc_table_69 },
	{ 0x10E4, 0x2C34, 0x3C34, 0x0000, 10, "MERIDIAN", NULL },
	{ 0x1609, 0x2C09, 0x3C09, 0x0000, 10, "CHANNEL 5 (2)", NULL },
	{ 0x25D0, 0x2C30, 0x3C30, 0x0000, 10, "WESTCOUNTRY TV", NULL },
	{ 0x28EB, 0x2C2B, 0x3C2B, 0x0000, 10, "CHANNEL 5 (3)", NULL },
	{ 0x2F27, 0x2C37, 0x3C37, 0x0000, 10, "CENTRAL TV", NULL },
	{ 0x3001, 0x2101, 0x3101, 0x0000, 11, "ET-1", NULL },
	{ 0x3002, 0x2102, 0x3102, 0x0000, 11, "NET", NULL },
	{ 0x3003, 0x2103, 0x3103, 0x0000, 11, "ET-3", NULL },
	{ 0x3101, 0x4801, 0x3801, 0x0000, 18, "Nederland 1", NULL },
	{ 0x3102, 0x4802, 0x3802, 0x0000, 18, "Nederland 2", NULL },
	{ 0x3103, 0x4803, 0x3803, 0x0000, 18, "Nederland 3", NULL },
	{ 0x3104, 0x4804, 0x3804, 0x0000, 18, "RTL 4", NULL },
	{ 0x3105, 0x4805, 0x3805, 0x0000, 18, "RTL 5", NULL },
	{ 0x3106, 0x4806, 0x3806, 0x0000, 18, "Yorin", NULL },
	{ 0x3120, 0x4820, 0x3820, 0x0000, 18, "The BOX", NULL },
	{ 0x3121, 0x0000, 0x0000, 0x0000, 18, "Discovery Netherlands", NULL },
	{ 0x3122, 0x0000, 0x0000, 0x0000, 18, "Nickelodeon", NULL },
	{ 0x3123, 0x0000, 0x0000, 0x0000, 18, "Animal Planet Benelux", NULL },
	{ 0x3124, 0x0000, 0x0000, 0x0000, 18, "TALPA TV", NULL },
	{ 0x3125, 0x0000, 0x0000, 0x0000, 18, "NET5", NULL },
	{ 0x3126, 0x0000, 0x0000, 0x0000, 18, "SBS6", NULL },
	{ 0x3128, 0x0000, 0x0000, 0x0000, 18, "V8", NULL },
	{ 0x3130, 0x0000, 0x0000, 0x0000, 18, "TMF Netherlands", NULL },
	{ 0x3131, 0x0000, 0x0000, 0x0000, 18, "TMF Belgian Flanders", NULL },
	{ 0x3132, 0x0000, 0x0000, 0x0000, 18, "MTV NL", NULL },
	{ 0x3137, 0x0000, 0x0000, 0x0000, 18, "RNN7", NULL },
	{ 0x3147, 0x4847, 0x3847, 0x0000, 18, "RTL 7", NULL },
	{ 0x3201, 0x1601, 0x3603, 0x0000,  2, "VRT TV1", NULL },
	{ 0x3202, 0x1602, 0x3602, 0x0000,  2, "CANVAS", NULL },
	{ 0x3203, 0x0000, 0x0000, 0x0000,  2, "RTBF 1", NULL },
	{ 0x3204, 0x0000, 0x0000, 0x0000,  2, "RTBF 2", NULL },
	{ 0x3205, 0x1605, 0x3605, 0x0000,  2, "VTM", NULL },
	{ 0x3206, 0x1606, 0x3606, 0x0000,  2, "Kanaal2", NULL },
	{ 0x3207, 0x0000, 0x0000, 0x0000,  2, "RTBF Sat", NULL },
	{ 0x3209, 0x0000, 0x0000, 0x0000,  2, "RTL-TV1", NULL },
	{ 0x320A, 0x0000, 0x0000, 0x0000,  2, "CLUB-RTL", NULL },
	{ 0x320B, 0x0000, 0x0000, 0x0000, 10, "National Geographic Channel", NULL },
	{ 0x320C, 0x0000, 0x0000, 0x0000,  2, "AB3", NULL },
	{ 0x320D, 0x0000, 0x0000, 0x0000,  2, "AB4e", NULL },
	{ 0x320E, 0x0000, 0x0000, 0x0000,  2, "Ring TV", NULL },
	{ 0x320F, 0x0000, 0x0000, 0x0000,  2, "JIM.tv", NULL },
	{ 0x3210, 0x0000, 0x0000, 0x0000,  2, "RTV-Kempen", NULL },
	{ 0x3211, 0x0000, 0x0000, 0x0000,  2, "RTV-Mechelen", NULL },
	{ 0x3212, 0x0000, 0x0000, 0x0000,  2, "MCM Belgium", NULL },
	{ 0x3213, 0x0000, 0x0000, 0x0000,  2, "Vitaya", NULL },
	{ 0x3214, 0x0000, 0x0000, 0x0000,  2, "WTV", NULL },
	{ 0x3215, 0x0000, 0x0000, 0x0000,  2, "FocusTV", NULL },
	{ 0x3216, 0x0000, 0x0000, 0x0000,  2, "Be 1 ana", NULL },
	{ 0x3217, 0x0000, 0x0000, 0x0000,  2, "Be 1 num", NULL },
	{ 0x3218, 0x0000, 0x0000, 0x0000,  2, "Be Ciné 1", NULL },
	{ 0x3219, 0x0000, 0x0000, 0x0000,  2, "Be Sport 1", NULL },
	{ 0x321A, 0x0000, 0x0000, 0x0000,  2, "PRIME Sport 1", NULL },
	{ 0x321B, 0x0000, 0x0000, 0x0000,  2, "PRIME Sport 2", NULL },
	{ 0x321C, 0x0000, 0x0000, 0x0000,  2, "PRIME Action", NULL },
	{ 0x321D, 0x0000, 0x0000, 0x0000,  2, "PRIME One", NULL },
	{ 0x321E, 0x0000, 0x0000, 0x0000,  2, "TV Brussel", NULL },
	{ 0x321F, 0x0000, 0x0000, 0x0000,  2, "AVSe", NULL },
	{ 0x3221, 0x0000, 0x0000, 0x0000,  2, "TV Limburg", NULL },
	{ 0x3222, 0x0000, 0x0000, 0x0000,  2, "Kanaal 3", NULL },
	{ 0x3223, 0x0000, 0x0000, 0x0000,  2, "ATV", NULL },
	{ 0x3224, 0x0000, 0x0000, 0x0000,  2, "ROB TV", NULL },
	{ 0x3225, 0x0000, 0x0000, 0x0000,  2, "PLUG TV", NULL },
	{ 0x3226, 0x0000, 0x0000, 0x0000,  2, "Sporza", NULL },
	{ 0x3230, 0x0000, 0x0000, 0x0000,  2, "Télé Bruxelles", NULL },
	{ 0x3231, 0x0000, 0x0000, 0x0000,  2, "Télésambre", NULL },
	{ 0x3232, 0x0000, 0x0000, 0x0000,  2, "TV Com", NULL },
	{ 0x3233, 0x0000, 0x0000, 0x0000,  2, "Canal Zoom", NULL },
	{ 0x3234, 0x0000, 0x0000, 0x0000,  2, "Vidéoscope", NULL },
	{ 0x3235, 0x0000, 0x0000, 0x0000,  2, "Canal C", NULL },
	{ 0x3236, 0x0000, 0x0000, 0x0000,  2, "Télé MB", NULL },
	{ 0x3237, 0x0000, 0x0000, 0x0000,  2, "Antenne Centre", NULL },
	{ 0x3238, 0x0000, 0x0000, 0x0000,  2, "Télévesdre", NULL },
	{ 0x3239, 0x0000, 0x0000, 0x0000,  2, "RTC Télé Liège", NULL },
	{ 0x3240, 0x0000, 0x0000, 0x0000,  2, "No tele", NULL },
	{ 0x3241, 0x0000, 0x0000, 0x0000,  2, "TV Lux", NULL },
	{ 0x325A, 0x0000, 0x0000, 0x0000,  2, "Kanaal Z - NL", NULL },
	{ 0x325B, 0x0000, 0x0000, 0x0000,  2, "CANAL Z - FR", NULL },
	{ 0x326A, 0x0000, 0x0000, 0x0000,  2, "CARTOON Network - NL", NULL },
	{ 0x326B, 0x0000, 0x0000, 0x0000,  2, "CARTOON Network - FR", NULL },
	{ 0x327A, 0x0000, 0x0000, 0x0000,  2, "LIBERTY CHANNEL - NL", NULL },
	{ 0x327B, 0x0000, 0x0000, 0x0000,  2, "LIBERTY CHANNEL - FR", NULL },
	{ 0x328A, 0x0000, 0x0000, 0x0000,  2, "TCM - NL", NULL },
	{ 0x328B, 0x0000, 0x0000, 0x0000,  2, "TCM - FR", NULL },
	{ 0x3298, 0x0000, 0x0000, 0x0000,  2, "Mozaiek/Mosaique", NULL },
	{ 0x3299, 0x0000, 0x0000, 0x0000,  2, "Info Kanaal/Canal Info", NULL },
	{ 0x32A7, 0x0000, 0x0000, 0x0000,  2, "Be 1 + 1h", NULL },
	{ 0x32A8, 0x0000, 0x0000, 0x0000,  2, "Be Ciné 2", NULL },
	{ 0x32A9, 0x0000, 0x0000, 0x0000,  2, "Be Sport 2", NULL },
	{ 0x330A, 0x2F0A, 0x3F0A, 0x0000,  9, "Arte", NULL },
	{ 0x3311, 0x2F11, 0x3F11, 0x0000,  9, "RFO1", NULL },
	{ 0x3312, 0x2F12, 0x3F12, 0x0000,  9, "RFO2", NULL },
	{ 0x3320, 0x2F20, 0x3F20, 0x0000,  9, "Aqui TV", NULL },
	{ 0x3321, 0x2F21, 0x3F21, 0x0000,  9, "TLM", NULL },
	{ 0x3322, 0x2F22, 0x3F22, 0x0000,  9, "TLT", NULL },
	{ 0x3333, 0x0000, 0x0000, 0x0000, 14, "TV3", NULL },
	{ 0x33B2, 0x0000, 0x0000, 0x0000,  9, "Sailing Channel", NULL },
	{ 0x33C1, 0x2FC1, 0x3F41, 0x0000,  9, "AB1", NULL },
	{ 0x33C2, 0x2FC2, 0x3F42, 0x0000,  9, "Canal J", NULL },
	{ 0x33C3, 0x2FC3, 0x3F43, 0x0000,  9, "Canal Jimmy", NULL },
	{ 0x33C4, 0x2FC4, 0x3F44, 0x0000,  9, "LCI", NULL },
	{ 0x33C5, 0x2FC5, 0x3F45, 0x0000,  9, "La Chaîne Météo", NULL },
	{ 0x33C6, 0x2FC6, 0x3F46, 0x0000,  9, "MCM", NULL },
	{ 0x33C7, 0x2FC7, 0x3F47, 0x0000,  9, "TMC Monte-Carlo", NULL },
	{ 0x33C8, 0x2FC8, 0x3F48, 0x0000,  9, "Paris Première", NULL },
	{ 0x33C9, 0x2FC9, 0x3F49, 0x0000,  9, "Planète", NULL },
	{ 0x33CA, 0x2FCA, 0x3F4A, 0x0000,  9, "Série Club", NULL },
	{ 0x33CB, 0x2FCB, 0x3F4B, 0x0000,  9, "Télétoon", NULL },
	{ 0x33CC, 0x2FCC, 0x3F4C, 0x0000,  9, "Téva", NULL },
	{ 0x33F1, 0x2F01, 0x3F01, 0x0000,  9, "TF1", NULL },
	{ 0x33F2, 0x2F02, 0x3F02, 0x0000,  9, "France 2", NULL },
	{ 0x33F3, 0x2F03, 0x3F03, 0x0000,  9, "France 3", NULL },
	{ 0x33F4, 0x2F04, 0x3F04, 0x0000,  9, "Canal+", NULL },
	{ 0x33F5, 0x2F05, 0x3F05, 0x0000,  9, "France 5", NULL },
	{ 0x33F6, 0x2F06, 0x3F06, 0x0000,  9, "M6", NULL },
	{ 0x3402, 0x0000, 0x0000, 0x0000,  7, "ETB 2", NULL },
	{ 0x3403, 0x0000, 0x0000, 0x0000,  7, "CANAL 9", NULL },
	{ 0x3404, 0x0000, 0x0000, 0x0000,  7, "PUNT 2", NULL },
	{ 0x3405, 0x0000, 0x0000, 0x0000,  7, "CCV", NULL },
	{ 0x340A, 0x0000, 0x0000, 0x0000,  7, "Arte", NULL },
	{ 0x3510, 0x0000, 0x0000, 0x0000, 21, "RTP1", NULL },
	{ 0x3511, 0x0000, 0x0000, 0x0000, 21, "RTP2", NULL },
	{ 0x3512, 0x0000, 0x0000, 0x0000, 21, "RTPAF", NULL },
	{ 0x3513, 0x0000, 0x0000, 0x0000, 21, "RTPI", NULL },
	{ 0x3514, 0x0000, 0x0000, 0x0000, 21, "RTPAZ", NULL },
	{ 0x3515, 0x0000, 0x0000, 0x0000, 21, "RTPM", NULL },
	{ 0x3531, 0x4201, 0x3201, 0x0000, 14, "RTE1", NULL },
	{ 0x3532, 0x4202, 0x3202, 0x0000, 14, "Network 2", NULL },
	{ 0x3533, 0x4203, 0x3203, 0x0000, 14, "Teilifis na Gaeilge", NULL },
	{ 0x3541, 0x0000, 0x0000, 0x0000, 15, "Rikisutvarpid-Sjonvarp", NULL },
	{ 0x3581, 0x2601, 0x3601, 0x0000,  8, "YLE1", NULL },
	{ 0x3582, 0x2602, 0x3607, 0x0000,  8, "YLE2", NULL },
	{ 0x358F, 0x260F, 0x3614, 0x0000,  8, "OWL3", NULL },
	{ 0x3601, 0x0000, 0x0000, 0x0000, 13, "MTV1", NULL },
	{ 0x3602, 0x0000, 0x0000, 0x0000, 13, "MTV2", NULL },
	{ 0x3611, 0x0000, 0x0000, 0x0000, 13, "MTV1 Budapest", NULL },
	{ 0x3621, 0x0000, 0x0000, 0x0000, 13, "MTV1 Pécs", NULL },
	{ 0x3622, 0x0000, 0x0000, 0x0000, 13, "tv2", NULL },
	{ 0x3631, 0x0000, 0x0000, 0x0000, 13, "MTV1 Szeged", NULL },
	{ 0x3636, 0x0000, 0x0000, 0x0000, 13, "Duna Televizio", NULL },
	{ 0x3641, 0x0000, 0x0000, 0x0000, 13, "MTV1 Szombathely", NULL },
	{ 0x3651, 0x0000, 0x0000, 0x0000, 13, "MTV1 Debrecen", NULL },
	{ 0x3661, 0x0000, 0x0000, 0x0000, 13, "MTV1 Miskolc", NULL },
	{ 0x3781, 0x0000, 0x0000, 0x0000, 25, "RTV", NULL },
	{ 0x37E5, 0x2C25, 0x3C25, 0x0000, 10, "SSVC", NULL },
	{ 0x3901, 0x0000, 0x0000, 0x0000, 16, "RAI 1", NULL },
	{ 0x3902, 0x0000, 0x0000, 0x0000, 16, "RAI 2", NULL },
	{ 0x3903, 0x0000, 0x0000, 0x0000, 16, "RAI 3", NULL },
	{ 0x3904, 0x0000, 0x0000, 0x0000, 16, "Rete A", NULL },
	{ 0x3905, 0x1505, 0x0000, 0x0000, 16, "Canale Italia", NULL },
	{ 0x3909, 0x1509, 0x0000, 0x0000, 16, "Telenova", NULL },
	{ 0x390A, 0x0000, 0x0000, 0x0000, 16, "Arte", NULL },
	{ 0x3910, 0x0000, 0x0000, 0x0000, 16, "TRS TV", NULL },
	{ 0x3911, 0x1511, 0x0000, 0x0000, 16, "Sky Cinema Classic", NULL },
	{ 0x3912, 0x1512, 0x0000, 0x0000, 16, "Sky Future use (canale 109)", NULL },
	{ 0x3913, 0x1513, 0x0000, 0x0000, 16, "Sky Calcio 1", NULL },
	{ 0x3914, 0x1514, 0x0000, 0x0000, 16, "Sky Calcio 2", NULL },
	{ 0x3915, 0x1515, 0x0000, 0x0000, 16, "Sky Calcio 3", NULL },
	{ 0x3916, 0x1516, 0x0000, 0x0000, 16, "Sky Calcio 4", NULL },
	{ 0x3917, 0x1517, 0x0000, 0x0000, 16, "Sky Calcio 5", NULL },
	{ 0x3918, 0x1518, 0x0000, 0x0000, 16, "Sky Calcio 6", NULL },
	{ 0x3919, 0x1519, 0x0000, 0x0000, 16, "Sky Calcio 7", NULL },
	{ 0x3920, 0x0000, 0x0000, 0x0000, 16, "RaiNews24", NULL },
	{ 0x3921, 0x0000, 0x0000, 0x0000, 16, "RAI Med", NULL },
	{ 0x3922, 0x0000, 0x0000, 0x0000, 16, "RAI Sport", NULL },
	{ 0x3923, 0x0000, 0x0000, 0x0000, 16, "RAI Educational", NULL },
	{ 0x3924, 0x0000, 0x0000, 0x0000, 16, "RAI Edu Lab", NULL },
	{ 0x3925, 0x0000, 0x0000, 0x0000, 16, "RAI Nettuno 1", NULL },
	{ 0x3926, 0x0000, 0x0000, 0x0000, 16, "RAI Nettuno 2", NULL },
	{ 0x3927, 0x0000, 0x0000, 0x0000, 16, "Camera Deputati", NULL },
	{ 0x3928, 0x0000, 0x0000, 0x0000, 16, "RAI Mosaico", NULL },
	{ 0x3930, 0x0000, 0x0000, 0x0000, 16, "Discovery Italy", NULL },
	{ 0x3933, 0x0000, 0x0000, 0x0000, 16, "MTV Italia", NULL },
	{ 0x3934, 0x0000, 0x0000, 0x0000, 16, "MTV Brand New", NULL },
	{ 0x3935, 0x0000, 0x0000, 0x0000, 16, "MTV Hits", NULL },
	{ 0x3938, 0x0000, 0x0000, 0x0000, 16, "RTV38", NULL },
	{ 0x3939, 0x0000, 0x0000, 0x0000, 16, "GAY TV", NULL },
	{ 0x3940, 0x0000, 0x0000, 0x0000, 16, "Video Italia", NULL },
	{ 0x3941, 0x0000, 0x0000, 0x0000, 16, "SAT 2000", NULL },
	{ 0x3942, 0x1542, 0x0000, 0x0000, 16, "Jimmy", NULL },
	{ 0x3943, 0x1543, 0x0000, 0x0000, 16, "Planet", NULL },
	{ 0x3944, 0x1544, 0x0000, 0x0000, 16, "Cartoon Network", NULL },
	{ 0x3945, 0x1545, 0x0000, 0x0000, 16, "Boomerang", NULL },
	{ 0x3946, 0x1546, 0x0000, 0x0000, 16, "CNN International", NULL },
	{ 0x3947, 0x1547, 0x0000, 0x0000, 16, "Cartoon Network +1", NULL },
	{ 0x3948, 0x1548, 0x0000, 0x0000, 16, "Sky Sports 3", NULL },
	{ 0x3949, 0x1549, 0x0000, 0x0000, 16, "Sky Diretta Gol", NULL },
	{ 0x3950, 0x0000, 0x0000, 0x0000, 16, "RaiSat Album", NULL },
	{ 0x3951, 0x0000, 0x0000, 0x0000, 16, "RaiSat Art", NULL },
	{ 0x3952, 0x0000, 0x0000, 0x0000, 16, "RaiSat Cinema", NULL },
	{ 0x3953, 0x0000, 0x0000, 0x0000, 16, "RaiSat Fiction", NULL },
	{ 0x3954, 0x0000, 0x0000, 0x0000, 16, "RaiSat GamberoRosso", NULL },
	{ 0x3955, 0x0000, 0x0000, 0x0000, 16, "RaiSat Ragazzi", NULL },
	{ 0x3956, 0x0000, 0x0000, 0x0000, 16, "RaiSat Show", NULL },
	{ 0x3957, 0x0000, 0x0000, 0x0000, 16, "RaiSat G. Rosso interattivo", NULL },
	{ 0x3960, 0x1560, 0x0000, 0x0000, 16, "SCI FI CHANNEL", NULL },
	{ 0x3961, 0x0000, 0x0000, 0x0000, 16, "Discovery Civilisations", NULL },
	{ 0x3962, 0x0000, 0x0000, 0x0000, 16, "Discovery Travel and Adventure", NULL },
	{ 0x3963, 0x0000, 0x0000, 0x0000, 16, "Discovery Science", NULL },
	{ 0x3968, 0x1568, 0x0000, 0x0000, 16, "Sky Meteo24", NULL },
	{ 0x3970, 0x0000, 0x0000, 0x0000, 16, "Sky Cinema 2", NULL },
	{ 0x3971, 0x0000, 0x0000, 0x0000, 16, "Sky Cinema 3", NULL },
	{ 0x3972, 0x0000, 0x0000, 0x0000, 16, "Sky Cinema Autore", NULL },
	{ 0x3973, 0x0000, 0x0000, 0x0000, 16, "Sky Cinema Max", NULL },
	{ 0x3974, 0x0000, 0x0000, 0x0000, 16, "Sky Cinema 16:9", NULL },
	{ 0x3975, 0x0000, 0x0000, 0x0000, 16, "Sky Sports 2", NULL },
	{ 0x3976, 0x0000, 0x0000, 0x0000, 16, "Sky TG24", NULL },
	{ 0x3977, 0x1577, 0x0000, 0x0000, 16, "Fox", NULL },
	{ 0x3978, 0x1578, 0x0000, 0x0000, 16, "Foxlife", NULL },
	{ 0x3979, 0x1579, 0x0000, 0x0000, 16, "National Geographic Channel", NULL },
	{ 0x3980, 0x1580, 0x0000, 0x0000, 16, "A1", NULL },
	{ 0x3981, 0x1581, 0x0000, 0x0000, 16, "History Channel", NULL },
	{ 0x3985, 0x0000, 0x0000, 0x0000, 16, "FOX Kids", NULL },
	{ 0x3986, 0x0000, 0x0000, 0x0000, 16, "PEOPLE TV - RETE 7", NULL },
	{ 0x3987, 0x0000, 0x0000, 0x0000, 16, "FOX Kids +1", NULL },
	{ 0x3988, 0x0000, 0x0000, 0x0000, 16, "LA7", NULL },
	{ 0x3989, 0x0000, 0x0000, 0x0000, 16, "PrimaTV", NULL },
	{ 0x398A, 0x0000, 0x0000, 0x0000, 16, "SportItalia", NULL },
	{ 0x3990, 0x1590, 0x0000, 0x0000, 16, "STUDIO UNIVERSAL", NULL },
	{ 0x3991, 0x0000, 0x0000, 0x0000, 16, "Marcopolo", NULL },
	{ 0x3992, 0x0000, 0x0000, 0x0000, 16, "Alice", NULL },
	{ 0x3993, 0x0000, 0x0000, 0x0000, 16, "Nuvolari", NULL },
	{ 0x3994, 0x0000, 0x0000, 0x0000, 16, "Leonardo", NULL },
	{ 0x3996, 0x1596, 0x0000, 0x0000, 16, "SUPERPIPPA CHANNEL", NULL },
	{ 0x3997, 0x0000, 0x0000, 0x0000, 16, "Tele+1", NULL },
	{ 0x3998, 0x0000, 0x0000, 0x0000, 16, "Tele+2", NULL },
	{ 0x3999, 0x0000, 0x0000, 0x0000, 16, "Tele+3", NULL },
	{ 0x39A0, 0x15A0, 0x0000, 0x0000, 16, "Sky Calcio 8", NULL },
	{ 0x39A1, 0x15A1, 0x0000, 0x0000, 16, "Sky Calcio 9", NULL },
	{ 0x39A2, 0x15A2, 0x0000, 0x0000, 16, "Sky Calcio 10", NULL },
	{ 0x39A3, 0x15A3, 0x0000, 0x0000, 16, "Sky Calcio 11", NULL },
	{ 0x39A4, 0x15A4, 0x0000, 0x0000, 16, "Sky Calcio 12", NULL },
	{ 0x39A5, 0x15A5, 0x0000, 0x0000, 16, "Sky Calcio 13", NULL },
	{ 0x39A6, 0x15A6, 0x0000, 0x0000, 16, "Sky Calcio 14", NULL },
	{ 0x39A7, 0x15A7, 0x0000, 0x0000, 16, "Telesanterno", NULL },
	{ 0x39A8, 0x15A8, 0x0000, 0x0000, 16, "Telecentro", NULL },
	{ 0x39A9, 0x15A9, 0x0000, 0x0000, 16, "Telestense", NULL },
	{ 0x39B0, 0x15B0, 0x0000, 0x0000, 16, "Disney Channel +1", NULL },
	{ 0x39B1, 0x0000, 0x0000, 0x0000, 16, "Sailing Channel", NULL },
	{ 0x39B2, 0x15B2, 0x0000, 0x0000, 16, "Disney Channel", NULL },
	{ 0x39B3, 0x15B3, 0x0000, 0x0000, 16, "7 Gold-Sestra Rete", NULL },
	{ 0x39B4, 0x15B4, 0x0000, 0x0000, 16, "Rete 8-VGA", NULL },
	{ 0x39B5, 0x15B5, 0x0000, 0x0000, 16, "Nuovarete", NULL },
	{ 0x39B6, 0x15B6, 0x0000, 0x0000, 16, "Radio Italia TV", NULL },
	{ 0x39B7, 0x15B7, 0x0000, 0x0000, 16, "Rete 7", NULL },
	{ 0x39B8, 0x15B8, 0x0000, 0x0000, 16, "E! Entertainment Television", NULL },
	{ 0x39B9, 0x15B9, 0x0000, 0x0000, 16, "Toon Disney", NULL },
	{ 0x39C7, 0x15C7, 0x0000, 0x0000, 16, "Bassano TV", NULL },
	{ 0x39C8, 0x15C8, 0x0000, 0x0000, 16, "ESPN Classic Sport", NULL },
	{ 0x39CA, 0x0000, 0x0000, 0x0000, 16, "VIDEOLINA", NULL },
	{ 0x39D1, 0x15D1, 0x0000, 0x0000, 16, "Mediaset Premium 5", NULL },
	{ 0x39D2, 0x15D2, 0x0000, 0x0000, 16, "Mediaset Premium 1", NULL },
	{ 0x39D3, 0x15D3, 0x0000, 0x0000, 16, "Mediaset Premium 2", NULL },
	{ 0x39D4, 0x15D4, 0x0000, 0x0000, 16, "Mediaset Premium 3", NULL },
	{ 0x39D5, 0x15D5, 0x0000, 0x0000, 16, "Mediaset Premium 4", NULL },
	{ 0x39D6, 0x15D6, 0x0000, 0x0000, 16, "BOING", NULL },
	{ 0x39D7, 0x15D7, 0x0000, 0x0000, 16, "Playlist Italia", NULL },
	{ 0x39D8, 0x15D8, 0x0000, 0x0000, 16, "MATCH MUSIC", NULL },
	{ 0x39E1, 0x15E1, 0x0000, 0x0000, 16, "National Geographic +1", NULL },
	{ 0x39E2, 0x15E2, 0x0000, 0x0000, 16, "Histroy Channel +1", NULL },
	{ 0x39E3, 0x15E3, 0x0000, 0x0000, 16, "Sky TV", NULL },
	{ 0x39E4, 0x15E4, 0x0000, 0x0000, 16, "GXT", NULL },
	{ 0x39E5, 0x15E5, 0x0000, 0x0000, 16, "Playhouse Disney", NULL },
	{ 0x39E6, 0x15E6, 0x0000, 0x0000, 16, "Sky Canale 224", NULL },
	{ 0x39F1, 0x0000, 0x0000, 0x0000, 16, "Teleradiocity", NULL },
	{ 0x39F2, 0x0000, 0x0000, 0x0000, 16, "Teleradiocity Genova", NULL },
	{ 0x39F3, 0x0000, 0x0000, 0x0000, 16, "Teleradiocity Lombardia", NULL },
	{ 0x39F4, 0x0000, 0x0000, 0x0000, 16, "Telestar Piemonte", NULL },
	{ 0x39F5, 0x0000, 0x0000, 0x0000, 16, "Telestar Liguria", NULL },
	{ 0x39F6, 0x0000, 0x0000, 0x0000, 16, "Telestar Lombardia", NULL },
	{ 0x39F7, 0x0000, 0x0000, 0x0000, 16, "Italia 8 Piemonte", NULL },
	{ 0x39F8, 0x0000, 0x0000, 0x0000, 16, "Italia 8 Lombardia", NULL },
	{ 0x3E00, 0x0000, 0x0000, 0x0000,  7, "TVE1", NULL },
	{ 0x4000, 0x0000, 0x0000, 0x0000, 17, "RTL Télé Letzebuerg", NULL },
	{ 0x4101, 0x24C1, 0x3441, 0x04C1,  3, "SRG Schweizer Fernsehen SF 1", NULL },
	{ 0x4102, 0x24C2, 0x3442, 0x04C2,  3, "SSR Télévis. Suisse TSR 1", NULL },
	{ 0x4103, 0x24C3, 0x3443, 0x04C3,  3, "SSR Televis. svizzera TSI 1", NULL },
	{ 0x4107, 0x24C7, 0x3447, 0x04C7,  3, "SRG Schweizer Fernsehen SF 2", NULL },
	{ 0x4108, 0x24C8, 0x3448, 0x04C8,  3, "SSR Télévis. Suisse TSR 2", NULL },
	{ 0x4109, 0x24C9, 0x3449, 0x04C9,  3, "SSR Televis. svizzera TSI 2", NULL },
	{ 0x410A, 0x24CA, 0x344A, 0x04CA,  3, "SRG SSR Sat Access", NULL },
	{ 0x4121, 0x2421, 0x0000, 0x0000,  3, "U1", NULL },
	{ 0x4201, 0x32C1, 0x3C21, 0x0000,  4, "CT 1", NULL },
	{ 0x4202, 0x32C2, 0x3C22, 0x0000,  4, "CT 2", NULL },
	{ 0x4203, 0x32C3, 0x3C23, 0x0000,  4, "NOVA TV", NULL },
	{ 0x4204, 0x0000, 0x0000, 0x0000,  4, "Prima TV", NULL },
	{ 0x4205, 0x0000, 0x0000, 0x0000,  4, "TV Praha", NULL },
	{ 0x4206, 0x0000, 0x0000, 0x0000,  4, "TV HK", NULL },
	{ 0x4207, 0x0000, 0x0000, 0x0000,  4, "TV Pardubice", NULL },
	{ 0x4208, 0x0000, 0x0000, 0x0000,  4, "TV Brno", NULL },
	{ 0x4211, 0x32D1, 0x3B01, 0x0000,  4, "CT1 Brno", NULL },
	{ 0x4212, 0x32D2, 0x3B04, 0x0000,  4, "CT2 Brno", NULL },
	{ 0x4221, 0x32E1, 0x3B02, 0x0000,  4, "CT1 Ostravia", NULL },
	{ 0x4222, 0x32E2, 0x3B05, 0x0000,  4, "CT2 Ostravia", NULL },
	{ 0x4231, 0x32F1, 0x3C25, 0x0000,  4, "CT1 Regional", NULL },
	{ 0x4232, 0x32F2, 0x3B03, 0x0000,  4, "CT2 Regional", NULL },
	{ 0x42A1, 0x35A1, 0x3521, 0x0000, 24, "STV1", NULL },
	{ 0x42A2, 0x35A2, 0x3522, 0x0000, 24, "STV2", NULL },
	{ 0x42A3, 0x35A3, 0x3523, 0x0000, 24, "STV1 Kosice", NULL },
	{ 0x42A4, 0x35A4, 0x3524, 0x0000, 24, "STV2 Kosice", NULL },
	{ 0x42A5, 0x35A5, 0x3525, 0x0000, 24, "STV1 B. Bystrica", NULL },
	{ 0x42A6, 0x35A6, 0x3526, 0x0000, 24, "STV2 B. Bystrica", NULL },
	{ 0x4301, 0x0000, 0x0000, 0x0AC1,  1, "ORF 1", pdc_table_193 },
	{ 0x4302, 0x0000, 0x0000, 0x0AC2,  1, "ORF 2", NULL },
	{ 0x430C, 0x0000, 0x0000, 0x0ACA,  1, "ATV+", pdc_table_205 },
	{ 0x4401, 0x5BFA, 0x3B7A, 0x0000, 10, "UK GOLD", NULL },
	{ 0x4402, 0x2C01, 0x3C01, 0x0000, 10, "UK LIVING", NULL },
	{ 0x4403, 0x2C3C, 0x3C3C, 0x0000, 10, "WIRE TV", NULL },
	{ 0x4404, 0x5BF0, 0x3B70, 0x0000, 10, "CHILDREN'S CHANNEL", NULL },
	{ 0x4405, 0x5BEF, 0x3B6F, 0x0000, 10, "BRAVO", NULL },
	{ 0x4406, 0x5BF7, 0x3B77, 0x0000, 10, "LEARNING CHANNEL", NULL },
	{ 0x4407, 0x5BF2, 0x3B72, 0x0000, 10, "DISCOVERY", NULL },
	{ 0x4408, 0x5BF3, 0x3B73, 0x0000, 10, "FAMILY CHANNEL", NULL },
	{ 0x4409, 0x5BF8, 0x3B78, 0x0000, 10, "Live TV", NULL },
	{ 0x4420, 0x0000, 0x0000, 0x0000, 10, "Discovery Home & Leisure", NULL },
	{ 0x4440, 0x2C40, 0x3C40, 0x0000, 10, "BBC2", NULL },
	{ 0x4441, 0x2C41, 0x3C41, 0x0000, 10, "BBC1 NI", NULL },
	{ 0x4442, 0x2C42, 0x3C42, 0x0000, 10, "BBC2 Wales", NULL },
	{ 0x4444, 0x2C44, 0x3C44, 0x0000, 10, "BBC2 Scotland", NULL },
	{ 0x4457, 0x2C57, 0x3C57, 0x0000, 10, "BBC World", NULL },
	{ 0x4468, 0x2C68, 0x3C68, 0x0000, 10, "BBC Prime", NULL },
	{ 0x4469, 0x2C69, 0x3C69, 0x0000, 10, "BBC News 24", NULL },
	{ 0x447B, 0x2C7B, 0x3C7B, 0x0000, 10, "BBC1 Scotland", NULL },
	{ 0x447D, 0x2C7D, 0x3C7D, 0x0000, 10, "BBC1 Wales", NULL },
	{ 0x447E, 0x2C7E, 0x3C7E, 0x0000, 10, "BBC2 NI", NULL },
	{ 0x447F, 0x2C7F, 0x3C7F, 0x0000, 10, "BBC1", NULL },
	{ 0x44C1, 0x0000, 0x0000, 0x0000, 10, "TNT / Cartoon Network", NULL },
	{ 0x44D1, 0x5BCC, 0x3B4C, 0x0000, 10, "DISNEY CHANNEL UK", NULL },
	{ 0x4502, 0x2902, 0x3902, 0x0000,  6, "TV2", NULL },
	{ 0x4503, 0x2904, 0x3904, 0x0000,  6, "TV2 Zulu", NULL },
	{ 0x4504, 0x0000, 0x0000, 0x0000,  6, "Discovery Denmark", NULL },
	{ 0x4505, 0x2905, 0x0000, 0x0000,  6, "TV2 Charlie", NULL },
	{ 0x4506, 0x2906, 0x0000, 0x0000,  6, "TV Danmark", NULL },
	{ 0x4507, 0x2907, 0x0000, 0x0000,  6, "Kanal 5", NULL },
	{ 0x4508, 0x2908, 0x0000, 0x0000,  6, "TV2 Film", NULL },
	{ 0x4600, 0x4E00, 0x3E00, 0x0000, 22, "SVT Test Transmissions", NULL },
	{ 0x4601, 0x4E01, 0x3E01, 0x0000, 22, "SVT 1", NULL },
	{ 0x4602, 0x4E02, 0x3E02, 0x0000, 22, "SVT 2", NULL },
	{ 0x4640, 0x4E40, 0x3E40, 0x0000, 22, "TV 4", NULL },
	{ 0x4701, 0x0000, 0x0000, 0x0000, 19, "NRK1", NULL },
	{ 0x4702, 0x0000, 0x0000, 0x0000, 19, "TV 2", NULL },
	{ 0x4703, 0x0000, 0x0000, 0x0000, 19, "NRK2", NULL },
	{ 0x4704, 0x0000, 0x0000, 0x0000, 19, "TV Norge", NULL },
	{ 0x4720, 0x0000, 0x0000, 0x0000, 19, "Discovery Nordic", NULL },
	{ 0x4801, 0x0000, 0x0000, 0x0000, 20, "TVP1", NULL },
	{ 0x4802, 0x0000, 0x0000, 0x0000, 20, "TVP2", NULL },
	{ 0x4810, 0x0000, 0x0000, 0x0000, 20, "TV Polonia", NULL },
	{ 0x4820, 0x0000, 0x0000, 0x0000, 20, "TVN", NULL },
	{ 0x4821, 0x0000, 0x0000, 0x0000, 20, "TVN Siedem", NULL },
	{ 0x4822, 0x0000, 0x0000, 0x0000, 20, "TVN24", NULL },
	{ 0x4830, 0x0000, 0x0000, 0x0000, 20, "Discovery Poland", NULL },
	{ 0x4831, 0x0000, 0x0000, 0x0000, 20, "Animal Planet", NULL },
	{ 0x4880, 0x0000, 0x0000, 0x0000, 20, "TVP Warszawa", NULL },
	{ 0x4881, 0x0000, 0x0000, 0x0000, 20, "TVP Bialystok", NULL },
	{ 0x4882, 0x0000, 0x0000, 0x0000, 20, "TVP Bydgoszcz", NULL },
	{ 0x4883, 0x0000, 0x0000, 0x0000, 20, "TVP Gdansk", NULL },
	{ 0x4884, 0x0000, 0x0000, 0x0000, 20, "TVP Katowice", NULL },
	{ 0x4886, 0x0000, 0x0000, 0x0000, 20, "TVP Krakow", NULL },
	{ 0x4887, 0x0000, 0x0000, 0x0000, 20, "TVP Lublin", NULL },
	{ 0x4888, 0x0000, 0x0000, 0x0000, 20, "TVP Lodz", NULL },
	{ 0x4890, 0x0000, 0x0000, 0x0000, 20, "TVP Rzeszow", NULL },
	{ 0x4891, 0x0000, 0x0000, 0x0000, 20, "TVP Poznan", NULL },
	{ 0x4892, 0x0000, 0x0000, 0x0000, 20, "TVP Szczecin", NULL },
	{ 0x4893, 0x0000, 0x0000, 0x0000, 20, "TVP Wroclaw", NULL },
	{ 0x4901, 0x0000, 0x3D41, 0x0DC1,  5, "ARD", pdc_table_117 },
	{ 0x4902, 0x0000, 0x3D42, 0x0DC2,  5, "ZDF", pdc_table_118 },
	{ 0x4908, 0x0000, 0x0000, 0x0DC8,  5, "Phoenix", NULL },
	{ 0x490A, 0x0000, 0x3D05, 0x0D85,  5, "Arte", pdc_table_60 },
	{ 0x490C, 0x0000, 0x0000, 0x0D8E,  5, "VOX", pdc_table_68 },
	{ 0x4941, 0x0000, 0x0000, 0x0D41,  5, "FESTIVAL", NULL },
	{ 0x4942, 0x0000, 0x0000, 0x0D42,  5, "MUXX", NULL },
	{ 0x4943, 0x0000, 0x0000, 0x0D43,  5, "EXTRA", NULL },
	{ 0x4944, 0x0000, 0x0000, 0x0000,  5, "BR-Alpha", NULL },
	{ 0x4982, 0x0000, 0x0000, 0x0D82,  5, "ORB-3", NULL },
	{ 0x49BD, 0x0000, 0x0000, 0x0D77,  5, "1-2-3.TV", NULL },
	{ 0x49BE, 0x0000, 0x0000, 0x0D78,  5, "TELE-5", NULL },
	{ 0x49BF, 0x0000, 0x0000, 0x0D7F,  5, "Home Shopping Europe", NULL },
	{ 0x49C7, 0x0000, 0x0000, 0x0DC7,  5, "3sat", pdc_table_122 },
	{ 0x49C9, 0x0000, 0x0000, 0x0DC9,  5, "Kinderkanal", NULL },
	{ 0x49CB, 0x0000, 0x3D4B, 0x0DCB,  5, "BR-3", pdc_table_126 },
	{ 0x49CF, 0x2903, 0x3903, 0x0000,  6, "DR2", NULL },
	{ 0x49D4, 0x0000, 0x0000, 0x0DD5,  5, "NDR-3 Dreiländerweit", NULL },
	{ 0x49D9, 0x0000, 0x0000, 0x0DDA,  5, "RB-3", NULL },
	{ 0x49DC, 0x0000, 0x0000, 0x0DDC,  5, "SFB-3", NULL },
	{ 0x49DF, 0x0000, 0x0000, 0x0DDF,  5, "SR-1 Regional", NULL },
	{ 0x49E1, 0x0000, 0x0000, 0x0DE1,  5, "SW 3 Baden-Württemberg", pdc_table_148 },
	{ 0x49E4, 0x0000, 0x0000, 0x0DE4,  5, "SW 3 Rheinland-Pfalz", NULL },
	{ 0x49E6, 0x0000, 0x0000, 0x0DE6,  5, "WDR-3", NULL },
	{ 0x49FE, 0x0000, 0x0000, 0x0DFE,  5, "MDR-3", pdc_table_177 },
	{ 0x49FF, 0x0000, 0x0000, 0x0DCF,  5, "Hessen 3", NULL },
	{ 0x4D54, 0x2C14, 0x3C14, 0x0000, 10, "MTV", NULL },
	{ 0x4D58, 0x2C20, 0x3C20, 0x0000, 10, "VH-1", NULL },
	{ 0x4D59, 0x2C21, 0x3C21, 0x0000, 10, "VH-1 German", NULL },
	{ 0x4D5A, 0x5BF4, 0x3B74, 0x0000, 10, "GRANADA PLUS", NULL },
	{ 0x4D5B, 0x5BF5, 0x3B75, 0x0000, 10, "GRANADA Timeshare", NULL },
	{ 0x5AAF, 0x2C3F, 0x3C3F, 0x0000, 10, "HTV", NULL },
	{ 0x5C44, 0x0000, 0x0000, 0x0000, 10, "QVC UK", NULL },
	{ 0x5C49, 0x0000, 0x0000, 0x0D7D,  5, "QVC", NULL },
	{ 0x7392, 0x2901, 0x3901, 0x0000,  6, "DR1", NULL },
	{ 0x7700, 0x0000, 0x0000, 0x07C0, 27, "1+1", NULL },
	{ 0x7705, 0x0000, 0x0000, 0x07C5, 27, "M1", NULL },
	{ 0x7707, 0x0000, 0x0000, 0x0000, 27, "ICTV", NULL },
	{ 0x7708, 0x0000, 0x0000, 0x07C8, 27, "Novy Kanal", NULL },
	{ 0x82DD, 0x2C1D, 0x3C1D, 0x0000, 10, "CARLTON TV", NULL },
	{ 0x82E1, 0x2C05, 0x3C05, 0x0000, 10, "CARLTON SELECT", NULL },
	{ 0x833B, 0x2C3D, 0x3C3D, 0x0000, 10, "ULSTER TV", NULL },
	{ 0x884B, 0x2C0B, 0x3C0B, 0x0000, 10, "LWT", NULL },
	{ 0x8E71, 0x2C31, 0x3C31, 0x0E86, 10, "NBC Europe", NULL },
	{ 0x8E72, 0x2C35, 0x3C35, 0x0000, 10, "CNBC Europe", NULL },
	{ 0x9001, 0x4301, 0x3301, 0x0000, 26, "TRT-1", NULL },
	{ 0x9002, 0x4302, 0x3302, 0x0000, 26, "TRT-2", NULL },
	{ 0x9003, 0x4303, 0x3303, 0x0000, 26, "TRT-3", NULL },
	{ 0x9004, 0x4304, 0x3304, 0x0000, 26, "TRT-4", NULL },
	{ 0x9005, 0x4305, 0x3305, 0x0000, 26, "TRT-INT", NULL },
	{ 0x9006, 0x4306, 0x3306, 0x0000, 26, "AVRASYA", NULL },
	{ 0x9007, 0x0000, 0x0000, 0x0000, 26, "Show TV", NULL },
	{ 0x9008, 0x0000, 0x0000, 0x0000, 26, "Cine 5", NULL },
	{ 0x9009, 0x0000, 0x0000, 0x0000, 26, "Super Sport", NULL },
	{ 0x900A, 0x0000, 0x0000, 0x0000, 26, "ATV", NULL },
	{ 0x900B, 0x0000, 0x0000, 0x0000, 26, "KANAL D", NULL },
	{ 0x900C, 0x0000, 0x0000, 0x0000, 26, "EURO D", NULL },
	{ 0x900D, 0x0000, 0x0000, 0x0000, 26, "EKO TV", NULL },
	{ 0x900E, 0x0000, 0x0000, 0x0000, 26, "BRAVO TV", NULL },
	{ 0x900F, 0x0000, 0x0000, 0x0000, 26, "GALAKSI TV", NULL },
	{ 0x9010, 0x0000, 0x0000, 0x0000, 26, "FUN TV", NULL },
	{ 0x9011, 0x0000, 0x0000, 0x0000, 26, "TEMPO TV", NULL },
	{ 0x9014, 0x0000, 0x0000, 0x0000, 26, "TGRT", NULL },
	{ 0x9020, 0x0000, 0x0000, 0x0000, 26, "STAR TV", NULL },
	{ 0x9021, 0x0000, 0x0000, 0x0000, 26, "STARMAX", NULL },
	{ 0x9022, 0x0000, 0x0000, 0x0000, 26, "KANAL 6", NULL },
	{ 0x9023, 0x0000, 0x0000, 0x0000, 26, "STAR 4", NULL },
	{ 0x9024, 0x0000, 0x0000, 0x0000, 26, "STAR 5", NULL },
	{ 0x9025, 0x0000, 0x0000, 0x0000, 26, "STAR 6", NULL },
	{ 0x9026, 0x0000, 0x0000, 0x0000, 26, "STAR 7", NULL },
	{ 0x9027, 0x0000, 0x0000, 0x0000, 26, "STAR 8", NULL },
	{ 0x9602, 0x2C02, 0x3C02, 0x0000, 10, "CHANNEL 5 (1)", NULL },
	{ 0xA460, 0x0000, 0x0000, 0x0000, 10, "Nickelodeon UK", NULL },
	{ 0xA465, 0x0000, 0x0000, 0x0000, 10, "Paramount Comedy Channel UK", NULL },
	{ 0xA55A, 0x0000, 0x0000, 0x0000,  7, "Canal+", NULL },
	{ 0xA82C, 0x2C2C, 0x3C2C, 0x0000, 10, "TYNE TEES TV", NULL },
	{ 0xAAE1, 0x0000, 0x0000, 0x0000, 23, "SLO1", NULL },
	{ 0xAAE2, 0x0000, 0x0000, 0x0000, 23, "SLO2", NULL },
	{ 0xAAE3, 0x0000, 0x0000, 0x0000, 23, "KC", NULL },
	{ 0xAAE4, 0x0000, 0x0000, 0x0000, 23, "TLM", NULL },
	{ 0xAAF1, 0x0000, 0x0000, 0x0000, 23, "SLO3", NULL },
	{ 0xADD8, 0x2C18, 0x3C18, 0x0000, 10, "GRANADA TV", NULL },
	{ 0xADDC, 0x5BD2, 0x3B52, 0x0000, 10, "GMTV", NULL },
	{ 0xB4C7, 0x2C07, 0x3C07, 0x0000, 10, "S4C", NULL },
	{ 0xB7F7, 0x2C27, 0x3C27, 0x0000, 10, "BORDER TV", NULL },
	{ 0xBA01, 0x0000, 0x0000, 0x0000,  7, "ETB 1", NULL },
	{ 0xC47B, 0x2C3B, 0x3C3B, 0x0000, 10, "CHANNEL 5 (4)", NULL },
	{ 0xC4F4, 0x42F4, 0x3274, 0x0000, 10, "FilmFour", NULL },
	{ 0xC8DE, 0x2C1E, 0x3C1E, 0x0000, 10, "ITV NETWORK", NULL },
	{ 0xCA03, 0x0000, 0x0000, 0x0000,  7, "TV3", NULL },
	{ 0xCA33, 0x0000, 0x0000, 0x0000,  7, "C33", NULL },
	{ 0xE100, 0x0000, 0x0000, 0x0000,  7, "TVE2", NULL },
	{ 0xE200, 0x0000, 0x0000, 0x0000,  7, "TVE Internacional Europa", NULL },
	{ 0xE500, 0x1FE5, 0x0000, 0x0000,  7, "Tele5", NULL },
	{ 0xF101, 0x2FE2, 0x3F62, 0x0000,  9, "Eurosport", NULL },
	{ 0xF102, 0x2FE3, 0x3F63, 0x0000,  9, "Eurosport2", NULL },
	{ 0xF103, 0x2FE4, 0x3F64, 0x0000,  9, "Eurosportnews", NULL },
	{ 0xF33A, 0x2C3A, 0x3C3A, 0x0000, 10, "GRAMPIAN TV", NULL },
	{ 0xF500, 0x2FE5, 0x3F65, 0x0000,  9, "TV5", NULL },
	{ 0xF9D2, 0x2C12, 0x3C12, 0x0000, 10, "SCOTTISH TV", NULL },
	{ 0xFA04, 0x0000, 0x0000, 0x0000, 16, "Rete 4", NULL },
	{ 0xFA05, 0x0000, 0x0000, 0x0000, 16, "Canale 5", NULL },
	{ 0xFA06, 0x0000, 0x0000, 0x0000, 16, "Italia 1", NULL },
	{ 0xFA08, 0x0000, 0x0000, 0x0000, 16, "TMC", NULL },
	{ 0xFA2C, 0x2C2D, 0x3C2D, 0x0000, 10, "YORKSHIRE TV", NULL },
	{ 0xFB9C, 0x2C1C, 0x3C1C, 0x0000, 10, "ANGLIA TV", NULL },
	{ 0xFC02, 0x1234, 0x0000, 0x0000,  0, "VGB V", NULL },
	{ 0xFCD1, 0x2C11, 0x3C11, 0x0000, 10, "CHANNEL 4", NULL },
	{ 0xFCE4, 0x2C24, 0x3C24, 0x0000, 10, "CHANNEL TV", NULL },
	{ 0xFCF3, 0x2C13, 0x3C13, 0x0000, 10, "RACING CHANNEL", NULL },
	{ 0xFCF4, 0x5BF6, 0x3B76, 0x0000, 10, "HISTORY CHANNEL", NULL },
	{ 0xFCF5, 0x2C15, 0x3C15, 0x0000, 10, "SCI FI CHANNEL", NULL },
	{ 0xFCF6, 0x5BF9, 0x3B79, 0x0000, 10, "SKY TRAVEL", NULL },
	{ 0xFCF7, 0x2C17, 0x3C17, 0x0000, 10, "SKY SOAPS", NULL },
	{ 0xFCF8, 0x2C08, 0x3C08, 0x0000, 10, "SKY SPORTS 2", NULL },
	{ 0xFCF9, 0x2C19, 0x3C19, 0x0000, 10, "SKY GOLD", NULL },
	{ 0xFCFA, 0x2C1A, 0x3C1A, 0x0000, 10, "SKY SPORTS", NULL },
	{ 0xFCFB, 0x2C1B, 0x3C1B, 0x0000, 10, "MOVIE CHANNEL", NULL },
	{ 0xFCFC, 0x2C0C, 0x3C0C, 0x0000, 10, "SKY MOVIES PLUS", NULL },
	{ 0xFCFD, 0x2C0D, 0x3C0D, 0x0000, 10, "SKY NEWS", NULL },
	{ 0xFCFE, 0x2C0E, 0x3C0E, 0x0000, 10, "SKY ONE", NULL },
	{ 0xFCFF, 0x2C0F, 0x3C0F, 0x0000, 10, "SKY TWO", NULL },
	{ 0xFE01, 0x2FE1, 0x3F61, 0x0000,  9, "Euronews", NULL },
};

/* 617 */

static const uint16_t
lookup_cni_8302 [] = {
	600, /* 1234 */
	279, /* 1505 */
	280, /* 1509 */
	283, /* 1511 */
	284, /* 1512 */
	285, /* 1513 */
	286, /* 1514 */
	287, /* 1515 */
	288, /* 1516 */
	289, /* 1517 */
	290, /* 1518 */
	291, /* 1519 */
	309, /* 1542 */
	310, /* 1543 */
	311, /* 1544 */
	312, /* 1545 */
	313, /* 1546 */
	314, /* 1547 */
	315, /* 1548 */
	316, /* 1549 */
	325, /* 1560 */
	329, /* 1568 */
	337, /* 1577 */
	338, /* 1578 */
	339, /* 1579 */
	340, /* 1580 */
	341, /* 1581 */
	348, /* 1590 */
	353, /* 1596 */
	357, /* 15A0 */
	358, /* 15A1 */
	359, /* 15A2 */
	360, /* 15A3 */
	361, /* 15A4 */
	362, /* 15A5 */
	363, /* 15A6 */
	364, /* 15A7 */
	365, /* 15A8 */
	366, /* 15A9 */
	367, /* 15B0 */
	369, /* 15B2 */
	370, /* 15B3 */
	371, /* 15B4 */
	372, /* 15B5 */
	373, /* 15B6 */
	374, /* 15B7 */
	375, /* 15B8 */
	376, /* 15B9 */
	377, /* 15C7 */
	378, /* 15C8 */
	380, /* 15D1 */
	381, /* 15D2 */
	382, /* 15D3 */
	383, /* 15D4 */
	384, /* 15D5 */
	385, /* 15D6 */
	386, /* 15D7 */
	387, /* 15D8 */
	388, /* 15E1 */
	389, /* 15E2 */
	390, /* 15E3 */
	391, /* 15E4 */
	392, /* 15E5 */
	393, /* 15E6 */
	158, /* 1601 */
	159, /* 1602 */
	127, /* 1604 */
	162, /* 1605 */
	163, /* 1606 */
	587, /* 1FE5 */
	136, /* 2101 */
	137, /* 2102 */
	138, /* 2103 */
	411, /* 2421 */
	404, /* 24C1 */
	405, /* 24C2 */
	406, /* 24C3 */
	407, /* 24C7 */
	408, /* 24C8 */
	409, /* 24C9 */
	410, /* 24CA */
	260, /* 2601 */
	261, /* 2602 */
	262, /* 260F */
	528, /* 2901 */
	458, /* 2902 */
	510, /* 2903 */
	459, /* 2904 */
	461, /* 2905 */
	462, /* 2906 */
	463, /* 2907 */
	464, /* 2908 */
	436, /* 2C01 */
	565, /* 2C02 */
	534, /* 2C05 */
	577, /* 2C07 */
	608, /* 2C08 */
	132, /* 2C09 */
	536, /* 2C0B */
	612, /* 2C0C */
	613, /* 2C0D */
	614, /* 2C0E */
	615, /* 2C0F */
	601, /* 2C11 */
	593, /* 2C12 */
	603, /* 2C13 */
	520, /* 2C14 */
	605, /* 2C15 */
	607, /* 2C17 */
	575, /* 2C18 */
	609, /* 2C19 */
	610, /* 2C1A */
	611, /* 2C1B */
	599, /* 2C1C */
	533, /* 2C1D */
	582, /* 2C1E */
	521, /* 2C20 */
	522, /* 2C21 */
	602, /* 2C24 */
	274, /* 2C25 */
	578, /* 2C27 */
	134, /* 2C2B */
	569, /* 2C2C */
	598, /* 2C2D */
	133, /* 2C30 */
	537, /* 2C31 */
	131, /* 2C34 */
	538, /* 2C35 */
	135, /* 2C37 */
	591, /* 2C3A */
	580, /* 2C3B */
	437, /* 2C3C */
	535, /* 2C3D */
	525, /* 2C3F */
	445, /* 2C40 */
	446, /* 2C41 */
	447, /* 2C42 */
	448, /* 2C44 */
	449, /* 2C57 */
	450, /* 2C68 */
	451, /* 2C69 */
	452, /* 2C7B */
	453, /* 2C7D */
	454, /* 2C7E */
	455, /* 2C7F */
	239, /* 2F01 */
	240, /* 2F02 */
	241, /* 2F03 */
	242, /* 2F04 */
	243, /* 2F05 */
	244, /* 2F06 */
	219, /* 2F0A */
	220, /* 2F11 */
	221, /* 2F12 */
	222, /* 2F20 */
	223, /* 2F21 */
	224, /* 2F22 */
	227, /* 2FC1 */
	228, /* 2FC2 */
	229, /* 2FC3 */
	230, /* 2FC4 */
	231, /* 2FC5 */
	232, /* 2FC6 */
	233, /* 2FC7 */
	234, /* 2FC8 */
	235, /* 2FC9 */
	236, /* 2FCA */
	237, /* 2FCB */
	238, /* 2FCC */
	616, /* 2FE1 */
	588, /* 2FE2 */
	589, /* 2FE3 */
	590, /* 2FE4 */
	592, /* 2FE5 */
	412, /* 32C1 */
	413, /* 32C2 */
	414, /* 32C3 */
	420, /* 32D1 */
	421, /* 32D2 */
	422, /* 32E1 */
	423, /* 32E2 */
	424, /* 32F1 */
	425, /* 32F2 */
	426, /* 35A1 */
	427, /* 35A2 */
	428, /* 35A3 */
	429, /* 35A4 */
	430, /* 35A5 */
	431, /* 35A6 */
	256, /* 4201 */
	257, /* 4202 */
	258, /* 4203 */
	581, /* 42F4 */
	539, /* 4301 */
	540, /* 4302 */
	541, /* 4303 */
	542, /* 4304 */
	543, /* 4305 */
	544, /* 4306 */
	139, /* 4801 */
	140, /* 4802 */
	141, /* 4803 */
	142, /* 4804 */
	143, /* 4805 */
	144, /* 4806 */
	145, /* 4820 */
	157, /* 4847 */
	465, /* 4E00 */
	466, /* 4E01 */
	467, /* 4E02 */
	468, /* 4E40 */
	457, /* 5BCC */
	576, /* 5BD2 */
	439, /* 5BEF */
	438, /* 5BF0 */
	125, /* 5BF1 */
	441, /* 5BF2 */
	442, /* 5BF3 */
	523, /* 5BF4 */
	524, /* 5BF5 */
	604, /* 5BF6 */
	440, /* 5BF7 */
	443, /* 5BF8 */
	606, /* 5BF9 */
	435, /* 5BFA */
};

static const uint16_t
lookup_cni_pdc_b [] = {
	136, /* 3101 */
	137, /* 3102 */
	138, /* 3103 */
	256, /* 3201 */
	257, /* 3202 */
	258, /* 3203 */
	581, /* 3274 */
	539, /* 3301 */
	540, /* 3302 */
	541, /* 3303 */
	542, /* 3304 */
	543, /* 3305 */
	544, /* 3306 */
	404, /* 3441 */
	405, /* 3442 */
	406, /* 3443 */
	407, /* 3447 */
	408, /* 3448 */
	409, /* 3449 */
	410, /* 344A */
	426, /* 3521 */
	427, /* 3522 */
	428, /* 3523 */
	429, /* 3524 */
	430, /* 3525 */
	431, /* 3526 */
	260, /* 3601 */
	159, /* 3602 */
	158, /* 3603 */
	127, /* 3604 */
	162, /* 3605 */
	163, /* 3606 */
	261, /* 3607 */
	262, /* 3614 */
	139, /* 3801 */
	140, /* 3802 */
	141, /* 3803 */
	142, /* 3804 */
	143, /* 3805 */
	144, /* 3806 */
	145, /* 3820 */
	157, /* 3847 */
	528, /* 3901 */
	458, /* 3902 */
	510, /* 3903 */
	459, /* 3904 */
	420, /* 3B01 */
	422, /* 3B02 */
	425, /* 3B03 */
	421, /* 3B04 */
	423, /* 3B05 */
	457, /* 3B4C */
	576, /* 3B52 */
	439, /* 3B6F */
	438, /* 3B70 */
	125, /* 3B71 */
	441, /* 3B72 */
	442, /* 3B73 */
	523, /* 3B74 */
	524, /* 3B75 */
	604, /* 3B76 */
	440, /* 3B77 */
	443, /* 3B78 */
	606, /* 3B79 */
	435, /* 3B7A */
	436, /* 3C01 */
	565, /* 3C02 */
	534, /* 3C05 */
	577, /* 3C07 */
	608, /* 3C08 */
	132, /* 3C09 */
	536, /* 3C0B */
	612, /* 3C0C */
	613, /* 3C0D */
	614, /* 3C0E */
	615, /* 3C0F */
	601, /* 3C11 */
	593, /* 3C12 */
	603, /* 3C13 */
	520, /* 3C14 */
	605, /* 3C15 */
	607, /* 3C17 */
	575, /* 3C18 */
	609, /* 3C19 */
	610, /* 3C1A */
	611, /* 3C1B */
	599, /* 3C1C */
	533, /* 3C1D */
	582, /* 3C1E */
	521, /* 3C20 */
	522, /* 3C21 */
	413, /* 3C22 */
	414, /* 3C23 */
	602, /* 3C24 */
	424, /* 3C25 */
	578, /* 3C27 */
	134, /* 3C2B */
	569, /* 3C2C */
	598, /* 3C2D */
	133, /* 3C30 */
	537, /* 3C31 */
	131, /* 3C34 */
	538, /* 3C35 */
	135, /* 3C37 */
	591, /* 3C3A */
	580, /* 3C3B */
	437, /* 3C3C */
	535, /* 3C3D */
	525, /* 3C3F */
	445, /* 3C40 */
	446, /* 3C41 */
	447, /* 3C42 */
	448, /* 3C44 */
	449, /* 3C57 */
	450, /* 3C68 */
	451, /* 3C69 */
	452, /* 3C7B */
	453, /* 3C7D */
	454, /* 3C7E */
	455, /* 3C7F */
	497, /* 3D05 */
	494, /* 3D41 */
	495, /* 3D42 */
	509, /* 3D4B */
	465, /* 3E00 */
	466, /* 3E01 */
	467, /* 3E02 */
	468, /* 3E40 */
	239, /* 3F01 */
	240, /* 3F02 */
	241, /* 3F03 */
	242, /* 3F04 */
	243, /* 3F05 */
	244, /* 3F06 */
	219, /* 3F0A */
	220, /* 3F11 */
	221, /* 3F12 */
	222, /* 3F20 */
	223, /* 3F21 */
	224, /* 3F22 */
	227, /* 3F41 */
	228, /* 3F42 */
	229, /* 3F43 */
	230, /* 3F44 */
	231, /* 3F45 */
	232, /* 3F46 */
	233, /* 3F47 */
	234, /* 3F48 */
	235, /* 3F49 */
	236, /* 3F4A */
	237, /* 3F4B */
	238, /* 3F4C */
	616, /* 3F61 */
	588, /* 3F62 */
	589, /* 3F63 */
	590, /* 3F64 */
	592, /* 3F65 */
};

static const uint16_t
lookup_cni_vps [] = {
	  0, /* 0481 */
	  1, /* 0482 */
	  2, /* 0483 */
	  3, /* 0484 */
	  4, /* 0485 */
	  5, /* 0486 */
	  6, /* 0487 */
	  7, /* 0488 */
	404, /* 04C1 */
	405, /* 04C2 */
	406, /* 04C3 */
	407, /* 04C7 */
	408, /* 04C8 */
	409, /* 04C9 */
	410, /* 04CA */
	529, /* 07C0 */
	530, /* 07C5 */
	532, /* 07C8 */
	432, /* 0AC1 */
	433, /* 0AC2 */
	  8, /* 0AC3 */
	434, /* 0ACA */
	  9, /* 0ACB */
	 10, /* 0ACC */
	 11, /* 0ACD */
	 12, /* 0ACE */
	 13, /* 0ACF */
	 14, /* 0AD0 */
	 15, /* 0AD1 */
	 16, /* 0AD2 */
	 17, /* 0AD3 */
	129, /* 0D14 */
	499, /* 0D41 */
	500, /* 0D42 */
	501, /* 0D43 */
	 18, /* 0D75 */
	 19, /* 0D76 */
	504, /* 0D77 */
	505, /* 0D78 */
	 20, /* 0D79 */
	 21, /* 0D7A */
	 22, /* 0D7B */
	 23, /* 0D7C */
	527, /* 0D7D */
	 24, /* 0D7E */
	506, /* 0D7F */
	 25, /* 0D81 */
	503, /* 0D82 */
	497, /* 0D85 */
	 26, /* 0D87 */
	 27, /* 0D88 */
	 28, /* 0D89 */
	 29, /* 0D8A */
	 30, /* 0D8B */
	 31, /* 0D8C */
	 32, /* 0D8D */
	498, /* 0D8E */
	130, /* 0D8F */
	 33, /* 0D90 */
	 34, /* 0D91 */
	 35, /* 0D92 */
	 36, /* 0D94 */
	 37, /* 0D95 */
	 38, /* 0D96 */
	 39, /* 0D97 */
	 40, /* 0D98 */
	 41, /* 0D99 */
	 42, /* 0D9A */
	 43, /* 0D9B */
	 44, /* 0D9C */
	 45, /* 0D9D */
	 46, /* 0D9E */
	 47, /* 0D9F */
	 48, /* 0DA0 */
	 49, /* 0DA1 */
	 50, /* 0DA2 */
	 51, /* 0DA3 */
	 52, /* 0DA4 */
	 53, /* 0DA5 */
	 54, /* 0DA6 */
	 55, /* 0DA7 */
	 56, /* 0DA8 */
	 57, /* 0DA9 */
	 58, /* 0DAA */
	 59, /* 0DAB */
	 60, /* 0DAC */
	 61, /* 0DAD */
	 62, /* 0DAE */
	 63, /* 0DAF */
	 64, /* 0DB0 */
	 65, /* 0DB1 */
	 66, /* 0DB2 */
	 67, /* 0DB3 */
	 68, /* 0DB4 */
	 69, /* 0DB5 */
	 70, /* 0DB6 */
	 71, /* 0DB7 */
	 72, /* 0DB8 */
	 73, /* 0DB9 */
	 74, /* 0DBA */
	 75, /* 0DBB */
	 76, /* 0DBD */
	 77, /* 0DBE */
	 78, /* 0DBF */
	494, /* 0DC1 */
	495, /* 0DC2 */
	 79, /* 0DC3 */
	 80, /* 0DC4 */
	 81, /* 0DC5 */
	507, /* 0DC7 */
	496, /* 0DC8 */
	508, /* 0DC9 */
	 82, /* 0DCA */
	509, /* 0DCB */
	 83, /* 0DCC */
	 84, /* 0DCD */
	 85, /* 0DCE */
	519, /* 0DCF */
	 86, /* 0DD0 */
	 87, /* 0DD1 */
	 88, /* 0DD2 */
	 89, /* 0DD3 */
	 90, /* 0DD4 */
	511, /* 0DD5 */
	 91, /* 0DD6 */
	 92, /* 0DD7 */
	 93, /* 0DD8 */
	 94, /* 0DD9 */
	512, /* 0DDA */
	 95, /* 0DDB */
	513, /* 0DDC */
	 96, /* 0DDD */
	 97, /* 0DDE */
	514, /* 0DDF */
	 98, /* 0DE0 */
	515, /* 0DE1 */
	 99, /* 0DE2 */
	100, /* 0DE3 */
	516, /* 0DE4 */
	101, /* 0DE5 */
	517, /* 0DE6 */
	102, /* 0DE7 */
	103, /* 0DE8 */
	104, /* 0DE9 */
	105, /* 0DEA */
	106, /* 0DEB */
	107, /* 0DEC */
	108, /* 0DED */
	109, /* 0DEE */
	110, /* 0DEF */
	111, /* 0DF0 */
	112, /* 0DF1 */
	113, /* 0DF2 */
	114, /* 0DF3 */
	115, /* 0DF4 */
	116, /* 0DF5 */
	117, /* 0DF6 */
	118, /* 0DF7 */
	119, /* 0DF8 */
	120, /* 0DF9 */
	121, /* 0DFA */
	122, /* 0DFB */
	123, /* 0DFC */
	124, /* 0DFD */
	518, /* 0DFE */
	537, /* 0E86 */
};

static const struct ttx_header
ttx_header_table [] = {
	{ "MTV", "### MTVtext   ##.##.##  " },
	{ "TW1", "###?TW1  " },
	{ "UPC Telekabel", "###?Wiener?Kabel?Text  " },
};

