#if 0

#define PRIVATE_STREAM_1 0xBD

typedef enum {
	DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE	= 0x02,
	DATA_UNIT_EBU_TELETEXT_SUBTITLE,

	DATA_UNIT_USER_DEFINED1_BEGIN		= 0x80,
	DATA_UNIT_USER_DEFINED1_END		= 0xC0,

	/* Libzvbi private, not defined in EN 301 775. */
	DATA_UNIT_ZVBI_WSS_CPR1204		= 0xB4,
	DATA_UNIT_ZVBI_CLOSED_CAPTION_525,
	DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525,

	DATA_UNIT_EBU_TELETEXT_INVERTED		= 0xC0,

	/* EN 301 775 Table 1 says this is Teletext data,
	   Table 3 says reserved. */
	DATA_UNIT_C1				= 0xC1,

	DATA_UNIT_VPS				= 0xC3,
	DATA_UNIT_WSS,
	DATA_UNIT_CLOSED_CAPTION,
	DATA_UNIT_MONOCHROME_SAMPLES,

	DATA_UNIT_USER_DEFINED2_BEGIN		= 0xC7,
	DATA_UNIT_USER_DEFINED2_END		= 0xFE,

	DATA_UNIT_STUFFING			= 0xFF
} data_unit;

/*
	Multiplexer
 */

/**
 * @param packet *packet is the output buffer pointer and will be
 *   advanced by the number of bytes stored.
 * @param packet_left *packet_left is the number of bytes left
 *   in the output buffer, and will be decremented by the number of
 *   bytes stored.
 * @param sliced *sliced is the vbi_sliced data to be multiplexed,
 *   and will be advanced by the number of sliced lines read.
 * @param sliced_left *sliced_left is the number of sliced lines
 *   left in the *sliced array, and will be decremented by the
 *   number of lines read.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be split or padded to data_unit_length 0x2C.
 * @param service_set Only data services in this set will be
 *   encoded, the rest is silently discarded. Create a set by
 *   or-ing @c VBI_SLICED_ values.
 *
 * Stores an array of vbi_sliced data in a MPEG-2 PES packet
 * as defined in EN 301 775. 
 *
 * The following data services can be encoded per EN 300 472:
 * @c VBI_SLICED_TELETEXT_B_625_L10 with data_unit_id 0x02. Additionally
 * EN 301 775 permits @c VBI_SLICED_VPS, @c _VPS_F2, @c _WSS_625
 * and @c _CAPTION_625 (any field).
 *
 * For completeness the function also encodes
 * @c VBI_SLICED_TELETEXT_B_625_L25, @c _CAPTION_525 (any field) and
 * @c _WSS_CPR1204 with data_unit_id 0x02, 0xB5 and 0xB4 respectively.
 * This behaviour can be modified with the service_set parameter.
 *
 * The lines in the sliced array must be sorted by ascending line
 * number and belong to the same frame. If unknown all lines can
 * have line number zero. Sliced data outside lines 1 ... 31 and 313 ... 344
 * (1 ... 31 and 263 ... 294 for NTSC data services) and data services
 * not covered will be silently discarded.
 *
 * When you call the function with *sliced == @c NULL or *sliced_left
 * == 0, all *packet_left bytes will be filled with stuffing bytes.
 *
 * @returns
 * @c FALSE if sliced lines are not sorted by ascending line number. 
 */
vbi_bool
vbi_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi_service_set	service_set)
{
	uint8_t *p;
	const vbi_sliced *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int last_line;
	vbi_bool fixed_length;

	assert (NULL != packet);
	assert (NULL != sliced);
	assert (NULL != packet_left);
	assert (NULL != sliced_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		goto finish;
	}

	s = *sliced;
	s_left = *sliced_left;

	if (NULL == s || 0 == s_left) {
		/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
		   stuffing byte: 0xFF */
		memset (p, 0xFF, p_left);

		p += p_left;
		p_left = 0;

		goto finish;
	}

	last_line = 0;

	fixed_length = (data_identifier >= 0x10 && data_identifier <= 0x1F);

	while (s_left > 0) {
		unsigned int length;
		unsigned int f2start;
		unsigned int i;

		if (s->line > 0) {
			if (s->line < last_line) {
				vbi_log_printf (VBI_DEBUG, __FUNCTION__,
						"Sliced lines not sorted by "
						"ascending line number\n");
				goto failure;
			}

			last_line = s->line;
		}

		if (!(s->id & service_set))
			goto skip;

		f2start = 313;

		if (s->id & (VBI_SLICED_CAPTION_525
			     | VBI_SLICED_WSS_CPR1204))
			f2start = 263;

		if (fixed_length) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & VBI_SLICED_TELETEXT_B_625) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & (VBI_SLICED_VPS | VBI_SLICED_VPS_F2)) {
			length = 2 + 1 + 13;
		} else if (s->id & (VBI_SLICED_WSS_625
				    | VBI_SLICED_CAPTION_525
				    | VBI_SLICED_CAPTION_625)) {
			length = 2 + 1 + 2;
		} else if (s->id & (VBI_SLICED_WSS_CPR1204)) {
			length = 2 + 1 + 3;
		} else {
			if (0)
				fprintf (stderr, "Skipping sliced id "
					 "0x%08x\n", s->id);
			goto skip;
		}

		if (length > p_left) {
			/* EN 301 775 section 4.3.1: Data units
			   cannot cross PES packet boundaries. */
			/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
			   stuffing byte: 0xFF */
			memset (p, 0xFF, p_left);

			p += p_left;
			p_left = 0;

			break;
		}

		if (s->line < 32) {
			/* Unknown line (0) or first field. */
			p[2] = (3 << 6) + (1 << 5) + sliced->line;
		} else if (s->line < f2start) {
			if (0)
				fprintf (stderr, "Sliced line %u exceeds "
					 "limit %u ... %u, %u ... %u\n",
					 s->line, 0, 31,
					 f2start, f2start + 31);
			goto skip;
		} else if (s->line < f2start + 32) {
			/* Second field. */
			p[2] = (3 << 6) + (0 << 5)
				+ sliced->line - f2start;
		} else {
			if (0)
				fprintf (stderr, "Sliced line %u exceeds "
					 "limit %u ... %u, %u ... %u\n",
					 s->line, 0, 31,
					 f2start, f2start + 31);
			goto skip;
		}

		if (s->id & VBI_SLICED_TELETEXT_B_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   framing_code [8], magazine_and_packet_address [16],
			   data_block [320] (msb first transmitted) */
			p[0] = DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE;
			p[1] = 44;
			p[3] = 0xE4; /* vbi_rev8 (0x27); */

			for (i = 0; i < 42; ++i)
				p[4 + i] = vbi_rev8 (s->data[i]);
		} else if (s->id & VBI_SLICED_CAPTION_525) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   data_block [16] (msb first) */
			p[0] = DATA_UNIT_ZVBI_CLOSED_CAPTION_525;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
		} else if (s->id & (VBI_SLICED_VPS | VBI_SLICED_VPS_F2)) {
			if ((s->id & (VBI_SLICED_VPS_F2))
			    && s->line < 32) {
				/* Indistinguishable from VBI_SLICED_VPS. */
				// XXX VBI_WARN
				goto skip;
			}

			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   vps_data_block [104] (msb first) */
			p[0] = DATA_UNIT_VPS;
			p[1] = 14;

			for (i = 0; i < 13; ++i)
				p[3 + i] = vbi_rev8 (s->data[i]);
		} else if (s->id & VBI_SLICED_WSS_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[14] (msb first), reserved[2] */
			p[0] = DATA_UNIT_WSS;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]) | 3;
		} else if (s->id & VBI_SLICED_CAPTION_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   data_block[16] (msb first) */
			p[0] = DATA_UNIT_CLOSED_CAPTIONING;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
		} else if (s->id & VBI_SLICED_WSS_CPR1204) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[20] (msb first), reserved[4] */
			p[0] = DATA_UNIT_ZVBI_WSS_CPR1204;
			p[1] = 4;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
			p[5] = vbi_rev8 (s->data[2]) | 0xF;
		} else {
			if (0)
				fprintf (stderr, "Skipping sliced id "
					 "0x%08x\n", s->id);
			// XXX VBI_WARN
			goto skip;
		}

		i = p[1] + 2;
		p += i;

		/* Pad to data_unit_length 0x2C if necessary. */
		while (i++ < length)
			*p++ = 0xFF;

		p_left -= length;

	skip:
		++s;
		--s_left;
	}

 finish:

	*packet = p;
	*packet_left = p_left;
	*sliced = s;
	*sliced_left = s_left;

	return TRUE;

 failure:

	*packet = p;
	*packet_left = p_left;
	*sliced = s;
	*sliced_left = s_left;

	return FALSE;
}

/**
 * @param packet *packet is the output buffer pointer and will be
 *   advanced by the number of bytes stored.
 * @param packet_left *packet_left is the number of bytes left
 *   in the output buffer, and will be decremented by the number of
 *   bytes stored.
 * @param samples *samples is the raw VBI data line to be
 *   multiplexed, in ITU-R BT.601 format, Y values only.
 *   *samples will be advanced by the number of bytes read.
 * @param samples_size Number of bytes in the *samples buffer.
 * @param samples_left *samples_left is the number of bytes left
 *   in the *samples buffer, and will be decremented by the
 *   number of bytes read.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be split or padded to data_unit_length 0x2C.
 * @param videostd_set Video standard.
 * @param line ITU-R line number (see vbi_sliced) to be encoded.
 *   Valid line numbers are 0 (unknown), 1 ... 31 and 313 ... 344
 *   for @c VBI_VIDEOSTD_SET_625, 1 ... 31 and 263 ... 294 for
 *   @c VBI_VIDEOSTD_SET_525.
 *
 * Stores one line of raw VBI data in a MPEG-2 PES packet
 * as defined in EN 301 775.
 *
 * EN 301 775 permits all video standards in the
 * @c VBI_VIDEOSTD_SET_625. Additionally the function accepts
 * @c VBI_VIDEOSTD_SET_525, these samples will be encoded
 * with data_unit_id 0xB6.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 */
vbi_bool
vbi_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		offset)
{
	uint8_t *p;
	const uint8_t *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int id;
	unsigned int f2start;
	unsigned int offset;
	vbi_bool fixed_length;

	assert (NULL != packet);
	assert (NULL != packet_left);
	assert (NULL != samples);
	assert (NULL != samples_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		return TRUE;
	}

	if (videostd_set & VBI_VIDEOSTD_SET_525) {
		if (videostd_set & VBI_VIDEOSTD_SET_625) {
			vbi_log_printf (VBI_DEBUG, __FUNCTION__,
					"Ambiguous videostd_set 0x%x\n",
					(unsigned int) videostd_set);
			return FALSE;
		}

		id = DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525;
		f2start = 263;
	} else {
		id = DATA_UNIT_MONOCHROME_SAMPLES;
		f2start = 313;
	}

	if (line < 32) {
		/* Unknown line (0) or first field. */
		line = (1 << 5) + line;
	} else if (line >= f2start && line < f2start + 32) {
		line = (0 << 5) + line - f2start;
	} else {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Line number %u exceeds limits "
				"%u ... %u, %u ... %u",
				line, 0, 31, f2start, f2start + 31);
		return FALSE;
	}

	s = *samples;
	s_left = *samples_left;

	if (offset + samples_size > 720) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"offset %u + samples_size %u > 720\n",
				offset, samples_size);
		return FALSE;
	}

	if (s_left > samples_size) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"samples_left %u > samples_size %u",
				s_left, samples_size);
		return FALSE;
	}

	fixed_length = (data_identifier >= 0x10 && data_identifier <= 0x1F);

	offset += samples_size - s_left;

	while (s_left > 0) {
		unsigned int offset;
		unsigned int n_pixels;

		if (p_left < 7) {
			/* EN 301 775 section 4.3.1: Data units
			   cannot cross PES packet boundaries. */
			/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
			   stuffing byte: 0xFF */
			memset (p, 0xFF, p_left);

			p += p_left;
			p_left = 0;

			goto finish;
		}

		if (fixed_length) {
			uint8_t *end;

			n_pixels = MIN (s_left, 2 + 0x2C - 6);
			n_pixels = MIN (n_pixels, p_left - 6);

			/* data_unit_id [8], data_unit_length [8],
			   first_segment_flag [1], last_segment_flag [1],
			   field_parity [1], line_offset [5],
			   first_pixel_position [16], n_pixels [8] */
			p[0] = id;
			p[1] = 0x2C;
			p[2] = (+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6)
				+ line);
			p[3] = offset >> 8;
			p[4] = offset;
			p[5] = n_pixels;

			memcpy (p + 6, s + offset, n_pixels);

			end = p + 2 + 0x2C;

			offset += n_pixels;

			n_pixels += 6;

			p += n_pixels;
			p_left -= n_pixels;

			/* Pad to data_unit_length 0x2C if necessary. */
			while (p < end)
				*p++ = 0xFF;
		} else {
			n_pixels = MIN (s_left, 251);
			n_pixels = MIN (n_pixels, p_left - 6);

			/* data_unit_id [8], data_unit_length [8],
			   first_segment_flag [1], last_segment_flag [1],
			   field_parity [1], line_offset [5],
			   first_pixel_position [16], n_pixels [8] */
			p[0] = id;
			p[1] = n_pixels + 4;
			p[2] = (+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6)
				+ line);
			p[3] = offset >> 8;
			p[4] = offset;
			p[5] = n_pixels;

			memcpy (p + 6, s + offset, n_pixels);

			offset += n_pixels;

			n_pixels += 6;

			p += n_pixels;
			p_left -= n_pixels;
		}

		s += n_pixels;
		s_left -= n_pixels;
	}

 finish:

	*packet = p;
	*packet_left = p_left;
	*samples = s;
	*samples_left = s_left;

	return TRUE;
}

struct vbi_dvb_demux {
	uint8_t			packet[65536 + 256];

	vbi_sliced		sliced[32];
	uint8_t			raw[32][720];

	vbi_sliced *		sp;
	uint8_t *		rp;

	unsigned int		last_line;

	unsigned int		raw_offset;
};

static vbi_sliced *
sliced				(vbi_dvb_demux *	dx,
				 unsigned int		lofp,
				 unsigned int		system)
{
	static const unsigned int start [2][2] = {
		{ 0, 263 },
		{ 0, 313 },
	};
	unsigned int line_offset;
	vbi_sliced *s;

	if (dx->sp >= dx->sliced + N_ELEMENTS (dx->sliced))
		return NULL;

	line_offset = lofp & 31;

	if (line_offset > 0) {
		unsigned int field_parity;
		unsigned int line;

		field_parity = !(lofp & (1 << 5));

		line = line_offset + start[system][field_parity];

		if (line <= dx->last_line) {
			/* EN 301 775 section 4.1: ascending line
			   order, no line twice. */
			// can we be more generous?
			return NULL;
		}

		dx->last_line = line;

		s = dx->sp++;
		s->line = line;
	} else {
		/* Unknown line. */

		s = dx->sp++;
		s->line = 0;
	}

	return s;
}

static vbi_bool
samples				(vbi_dvb_demux *	dx,
				 uint8_t *		p,
				 unsigned int		system)
{
	vbi_sliced *s;
	unsigned int offset;
	unsigned int n_pixels;
	unsigned int line;

	if (!(s = sliced (dx, p[2], system)))
		return FALSE;

	offset = p[3] * 256 + p[4];
	n_pixels = p[5];

	if (n_pixels < 1 || (offset + n_pixels) > 720) {
		return FALSE;
	}

	if (p[2] & (1 << 7)) {
		/* First segment. */
		if (dx->raw_offset > 0) {
			/* Discard incomplete line. */
			memset (dx->rp, 0, 720);
		}
	} else {
		if (0 == dx->raw_offset) {
			/* Continuity lost. */
			return TRUE;
		} else if (dx->raw_offset != offset) {
			/* Continuity lost, discard incomplete line. */
			memset (dx->rp, 0, 720);
			dx->raw_offset = 0;
			return TRUE;
		}
	}

	s->id = VBI_SLICED_NONE;

	dx->rp = &dx_raw[s - dx->sliced];
	memcpy (dx->rp + offset, p + 6, n_pixels);

	if (p[2] & (1 << 6)) {
		/* Last segment. */
		dx->raw_offset = 0;
	} else {
		dx->raw_offset = offset + n_pixels;
	}

	return TRUE;
}

static vbi_bool
data_field			(vbi_dvb_demux *	dx,
				 uint8_t *		p,
				 unsigned int		left)
{
	while (left > 0) {
		unsigned int data_unit_id;
		unsigned int data_unit_length;
		vbi_sliced *s;
		unsigned int i;

		data_unit_id = p[0];

		if (DATA_UNIT_STUFFING == data_unit_id) {
			++p;
			--left;
			continue;
		}

		data_unit_length = p[1];

		/* ETSI EN 301 775 section 4.3.1: Data units cannot cross
		   PES packet boundaries. */
		if (left < data_unit_length + 2)
			return FALSE;

		switch (data_unit_id) {
		case DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE:
		case DATA_UNIT_EBU_TELETEXT_SUBTITLE:
			if (data_unit_length != 1 + 1 + 42)
				return FALSE;

			if (0xE4 != p[3]) /* vbi_rev8 (0x27) */
				break;

			if (!(s = sliced (dx, p[2], 1)))
				return FALSE;

			s->id = VBI_SLICED_TELETEXT_B_625;

			for (i = 0; i < 42; ++i)
				s->data[i] = vbi_rev8 (p[4 + i]);

			break;

		case DATA_UNIT_VPS:
			if (data_unit_length < 1 + 13)
				return FALSE;

			if (!(s = sliced (dx, p[2], 1)))
				return FALSE;

			s->id = (s->line >= 313) ?
				VBI_SLICED_VPS_F2 : VBI_SLICED_VPS;

			for (i = 0; i < 13; ++i)
				s->data[i] = vbi_rev8 (p[3 + i]);

			break;

		case DATA_UNIT_WSS:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced (dx, p[2], 1)))
				return FALSE;

			s->id = VBI_SLICED_WSS_625;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

		case DATA_UNIT_ZVBI_WSS_CPR1204:
			if (data_unit_length < 1 + 3)
				return FALSE;

			if (!(s = sliced (dx, p[2], 0)))
				return FALSE;

			s->id = VBI_SLICED_WSS_CPR1204;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);
			s->data[2] = vbi_rev8 (p[5]);

			break;

		case DATA_UNIT_ZVBI_CAPTION_625:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced (dx, p[2], 0)))
				return FALSE;

			s->id = VBI_SLICED_CAPTION_525;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

		case DATA_UNIT_CLOSED_CAPTIONING:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced (dx, p[2], 1)))
				return FALSE;

			s->id = VBI_SLICED_CAPTION_625;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

		case DATA_UNIT_MONOCHROME_SAMPLES:
			if (data_unit_length < p[5] + 4)
				return FALSE;

			if (!samples (dx, p, 1))
				return FALSE;

			break;

		case DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525:
			if (data_unit_length < p[5] + 4)
				return FALSE;

			if (!samples (dx, p, 0))
				return FALSE;

			break;

		default:
			break;
		}

		data_unit_length += 2;

		p += data_unit_length;
		left -= data_unit_length;
	}

	return TRUE;
}

static vbi_bool
timestamp			(uint64_t *		pts,
				 unsigned int		mark,
				 const uint8_t *	p)
{
	unsigned int t;

	t = p[0];

	if (mark != (t & 0xF1))
		return FALSE;

	*pts = ((uint64_t)(t & 0x0E)) << (30 - 1);

	t  =  p[1] << 23;
	t |= (p[2] & ~1) << (15 - 1);
	t |=  p[3] << 8;
	t |= (p[4] & ~1) >> 1;

	*pts |= t;

	return TRUE;
}

static vbi_bool
scan				(vbi_dvb_demux *	dx,
				 uint64_t *		pts,
				 const uint8_t *	p,
				 const uint8_t *	end)
{
	unsigned int packet_length;
	unsigned int avail;

	while (p < end) {
		/* packet_start_code_prefix [24] == 0x000001,
		   stream_id [8] == PRIVATE_STREAM_1 */

		if (p[2] & ~1) {
			/* Not 000001 or xx0000 or xxxx00. */
			p += 3;
		} else if (0 != p[0] ||
			   0 != p[1] ||
			   1 != p[2]) {
			++p;
		} else if (PRIVATE_STREAM_1 == p[3]) {
			/* '10', PES_scrambling_control [2] == 0, PES_priority,
			   data_alignment_indicator == 1, copyright,
			   original_or_copy */
			if (0x84 != (p[6] & 0xF4)) {
				goto skip_packet;
			}

			packet_length = p[4] * 256 + p[5];

			switch (p[7] >> 6) {
			case 2: /* PTS 0010 xxx 1 ... */
				if (timestamp (&dx->pts, 0x21, p + 9))
					return TRUE;

			case 3: /* PTS 0011 xxx 1 ... DTS ... */
				if (timestamp (&dx->pts, 0x31, p + 9))
					return TRUE;

			default:
				*pts = 0;
				return TRUE;
			}
		} else if (p[3] >= 0xBC) {
		skip_packet:
			packet_length =	p[4] * 256 + p[5]
				+ 4 /* start code */;

			avail = end - p;

			if (packet_length >= avail) {
				dx->skip = packet_length - avail;
				break;
			} else {
				p += packet_length;
			}
		}
	}
}

#if 0
				(const uint8_t **	pes,
				 unsigned int *		pes_left)
{
	uint8_t *p;
	unsigned int *p_left;

	p = *pes;
	p_left = *pes_left;

	for (;;) {
		if (dx->need > 0) {
			if (dx->need > p_left) {
				memcpy (dx->wp, p, p_left);

				dx->wp += p_left;
				dx->need -= p_left;

				*pes = p + p_left;
				*pes_left = 0;
				return;
			}

			memcpy (dx->wp, p, dx->need);

			/* Now we have the last dx->wp + dx->need - dx->buffer
			   bytes from previous frames plus dx->lookahead bytes
			   from the current frame. */
			p2 = dx->wrap_buffer;
			end = dx->wp + dx->need - dx->lookahead;

			dx->need = 0;
		} else {
			if (dx->skip > 0) {
				if (dx->skip > p_left) {
					dx->skip -= p_left;

					*pes = p + p_left;
					*pes_left = 0;
					return;
				}

				p += dx->skip;
				p_left -= dx->skip;

				dx->skip = 0;
			}

			if (dx->lookahead > p_left) {
				memcpy (dx->wrap_buffer, p, p_left);

				dx->wp = dx->wrap_buffer + p_left;

				/* Since p_left > 0 actually
				   dx->lookahead - 1 would suffice. */
				dx->need = dx->lookahead;

				*pes = p + p_left;
				*pes_left = 0;
				return;
			}

			p2 = p;
			end2 = p + p_left - dx->lookahead + 1;
		}

		if (resync) {
			uint64_t pts;

			if (!scan (dx, &pts, p2, end2)) {
				if (dx->wp) {
					/* Not found in dx->wrap_buffer,
					   continue in current frame. */
					dx->wp = NULL;
					continue;
				} else {
					/* Not found in current frame. */
					p = end2;
					p_left = dx->lookahead - 1;

					memcpy (dx->wrap_buffer, p, p_left);

					dx->wp = dx->wrap_buffer + p_left;
					dx->need = dx->lookahead;

					*pes = p + p_left;
					*pes_left = 0;
					return;
				}
			}

			what now? 
		}
	}
	
#endif
#endif
