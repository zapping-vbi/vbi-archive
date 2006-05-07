#if 0






/**
 * @internal
 */
#define PRIVATE_STREAM_1 0xBD

/**
 * @internal
 * EN 301 775, Section 4.3.2, Table 2 data_identifier.
 */
typedef enum {
	/* 0x00 ... 0x0F reserved. */

	/* Teletext combined with VPS and/or WSS and/or CC
	   and/or VBI sample data. (EN 300 472, 301 775) */
	DATA_ID_EBU_TELETEXT_BEGIN		= 0x10,
	DATA_ID_EBU_TELETEXT_END		= 0x20,

	/* 0x20 ... 0x7F reserved. */

	DATA_ID_USER_DEFINED1_BEGIN		= 0x80,
	DATA_ID_USER_DEFINED2_END		= 0x99,

	/* Teletext and/or VPS and/or WSS and/or CC and/or
	   VBI sample data. (EN 301 775) */
	DATA_ID_EBU_DATA_BEGIN			= 0x99,
	DATA_ID_EBU_DATA_END			= 0x9C,

	/* 0x9C ... 0xFF reserved. */
} data_identifier;

/**
 * @internal
 * EN 301 775, Section 4.3.2, Table 3 data_unit_id.
 */
typedef enum {
	/* 0x00 ... 0x01 reserved. */

	DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE	= 0x02,
	DATA_UNIT_EBU_TELETEXT_SUBTITLE,

	/* 0x04 ... 0x7F reserved. */

	DATA_UNIT_USER_DEFINED1_BEGIN		= 0x80,
	DATA_UNIT_USER_DEFINED1_END		= 0xC0,

	/* Libzvbi private, not defined in EN 301 775. */
	DATA_UNIT_ZVBI3_WSS_CPR1204		= 0xB4,
	DATA_UNIT_ZVBI3_CLOSED_CAPTION_525,
	DATA_UNIT_ZVBI3_MONOCHROME_SAMPLES_525,

	DATA_UNIT_EBU_TELETEXT_INVERTED		= 0xC0,

	/* EN 301 775 Table 1 says this is Teletext data,
	   Table 3 says reserved. */
	DATA_UNIT_C1				= 0xC1,

	/* 0xC2 reserved. */

	DATA_UNIT_VPS				= 0xC3,
	DATA_UNIT_WSS,
	DATA_UNIT_CLOSED_CAPTION,
	DATA_UNIT_MONOCHROME_SAMPLES,

	DATA_UNIT_USER_DEFINED2_BEGIN		= 0xC7,
	DATA_UNIT_USER_DEFINED2_END		= 0xFE,

	DATA_UNIT_STUFFING			= 0xFF
} data_unit_id;

/*
	Multiplexer
*/

/**
 * @param packet *packet is the output buffer pointer and will be
 *   advanced by the number of bytes stored.
 * @param packet_left *packet_left is the number of bytes left
 *   in the output buffer, and will be decremented by the number of
 *   bytes stored.
 * @param sliced *sliced is the vbi3_sliced data to be multiplexed,
 *   and will be advanced by the number of sliced lines read.
 * @param sliced_left *sliced_left is the number of sliced lines
 *   left in the *sliced array, and will be decremented by the
 *   number of lines read.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be split or padded to data_unit_length 0x2C.
 * @param service_set Only data services in this set will be
 *   encoded. Create a set by or-ing @c VBI3_SLICED_ values.
 *
 * Stores an array of vbi3_sliced data in a MPEG-2 PES packet
 * as defined in EN 301 775. When *sliced_left is zero, or when
 * *packet_left becomes too small the function fills the remaining
 * space with stuffing bytes.
 *
 * The following data services can be encoded per EN 300 472:
 * @c VBI3_SLICED_TELETEXT_B_625_L10 with data_unit_id 0x02. Additionally
 * EN 301 775 permits @c VBI3_SLICED_VPS, @c _VPS_F2, @c _WSS_625
 * and @c _CAPTION_625 (any field).
 *
 * For completeness the function also encodes
 * @c VBI3_SLICED_TELETEXT_B_625_L25, @c _CAPTION_525 (any field) and
 * @c _WSS_CPR1204 with data_unit_id 0x02, 0xB5 and 0xB4 respectively.
 * You can modify this behaviour with the service_set parameter.
 *
 * The lines in the sliced array must be sorted by ascending line
 * number and belong to the same frame. If unknown all lines can
 * have line number zero. Sliced data outside lines 1 ... 31 and 313 ... 344
 * (1 ... 31 and 263 ... 294 for NTSC data services) and data services
 * not covered will be ignored.
 */
void
vbi3_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi3_service_set	service_set)
{
	uint8_t *p;
	const vbi3_sliced *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int last_line;
	vbi3_bool fixed_length;

	assert (NULL != packet);
	assert (NULL != sliced);
	assert (NULL != packet_left);
	assert (NULL != sliced_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		return;
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

	fixed_length = (data_identifier >= DATA_ID_EBU_TELETEXT_BEGIN
			&& data_identifier < DATA_ID_EBU_TELETEXT_END);

	while (s_left > 0) {
		unsigned int length;
		unsigned int f2start;
		unsigned int i;

		if (s->line > 0) {
			if (s->line < last_line) {
				fprintf (stderr, __FUNCTION__,
					 "%s: Sliced lines not sorted.\n");
				abort ();
			}

			last_line = s->line;
		}

		if (!(s->id & service_set))
			goto skip;

		f2start = 313;

		if (s->id & (VBI3_SLICED_CAPTION_525 |
			     VBI3_SLICED_WSS_CPR1204))
			f2start = 263;

		if (fixed_length) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & VBI3_SLICED_TELETEXT_B_625) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & (VBI3_SLICED_VPS |
				    VBI3_SLICED_VPS_F2)) {
			length = 2 + 1 + 13;
		} else if (s->id & (VBI3_SLICED_WSS_625 |
				    VBI3_SLICED_CAPTION_525 |
				    VBI3_SLICED_CAPTION_625)) {
			length = 2 + 1 + 2;
		} else if (s->id & (VBI3_SLICED_WSS_CPR1204)) {
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

		if (s->id & VBI3_SLICED_TELETEXT_B_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   framing_code [8], magazine_and_packet_address [16],
			   data_block [320] (msb first transmitted) */
			p[0] = DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE;
			p[1] = 44;
			p[3] = 0xE4; /* vbi3_rev8 (0x27); */

			for (i = 0; i < 42; ++i)
				p[4 + i] = vbi3_rev8 (s->data[i]);
		} else if (s->id & VBI3_SLICED_CAPTION_525) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   data_block [16] (msb first) */
			p[0] = DATA_UNIT_ZVBI3_CLOSED_CAPTION_525;
			p[1] = 3;
			p[3] = vbi3_rev8 (s->data[0]);
			p[4] = vbi3_rev8 (s->data[1]);
		} else if (s->id & (VBI3_SLICED_VPS | VBI3_SLICED_VPS_F2)) {
			if ((s->id & (VBI3_SLICED_VPS_F2))
			    && s->line < 32) {
				/* Indistinguishable from VBI3_SLICED_VPS. */
				// XXX VBI3_WARN
				goto skip;
			}

			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   vps_data_block [104] (msb first) */
			p[0] = DATA_UNIT_VPS;
			p[1] = 14;

			for (i = 0; i < 13; ++i)
				p[3 + i] = vbi3_rev8 (s->data[i]);
		} else if (s->id & VBI3_SLICED_WSS_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[14] (msb first), reserved[2] */
			p[0] = DATA_UNIT_WSS;
			p[1] = 3;
			p[3] = vbi3_rev8 (s->data[0]);
			p[4] = vbi3_rev8 (s->data[1]) | 3;
		} else if (s->id & VBI3_SLICED_CAPTION_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   data_block[16] (msb first) */
			p[0] = DATA_UNIT_CLOSED_CAPTIONING;
			p[1] = 3;
			p[3] = vbi3_rev8 (s->data[0]);
			p[4] = vbi3_rev8 (s->data[1]);
		} else if (s->id & VBI3_SLICED_WSS_CPR1204) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[20] (msb first), reserved[4] */
			p[0] = DATA_UNIT_ZVBI3_WSS_CPR1204;
			p[1] = 4;
			p[3] = vbi3_rev8 (s->data[0]);
			p[4] = vbi3_rev8 (s->data[1]);
			p[5] = vbi3_rev8 (s->data[2]) | 0xF;
		} else {
			if (0)
				fprintf (stderr, "Skipping sliced id "
					 "0x%08x\n", s->id);
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
 * @param samples_left *samples_left is the number of bytes left
 *   in the *samples buffer, and will be decremented by the
 *   number of bytes read.
 * @param offset Offset of the first sample in the *samples buffer.
 * @param samples_size Number of bytes in the *samples buffer.
 *   offset + samples_size must not exceed 720.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be restricted to data_unit_length 0x2C.
 * @param videostd_set Video standard.
 * @param line ITU-R line number to be encoded (see vbi3_sliced).
 *   Valid line numbers are 0 (unknown), 1 ... 31 and 313 ... 344
 *   for @c VBI3_VIDEOSTD_SET_625, 1 ... 31 and 263 ... 294 for
 *   @c VBI3_VIDEOSTD_SET_525.
 *
 * Stores one line of raw VBI data in a MPEG-2 PES packet
 * as defined in EN 301 775. When *packet_left becomes too small
 * the function fills up the remaining space with stuffing bytes.
 *
 * EN 301 775 permits all video standards in the
 * @c VBI3_VIDEOSTD_SET_625. Additionally the function accepts
 * @c VBI3_VIDEOSTD_SET_525, these samples will be encoded
 * with data_unit_id 0xB6.
 */
void
vbi3_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		offset,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi3_videostd_set	videostd_set,
				 unsigned int		line)
{
	uint8_t *p;
	const uint8_t *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int id;
	unsigned int f2start;
	unsigned int offset;
	unsigned int min_space;

	assert (NULL != packet);
	assert (NULL != packet_left);
	assert (NULL != samples);
	assert (NULL != samples_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		return;
	}

	if (videostd_set & VBI3_VIDEOSTD_SET_525) {
		if (videostd_set & VBI3_VIDEOSTD_SET_625) {
			fprintf (stderr, __FUNCTION__,
				 "%s: Ambiguous videostd_set 0x%x\n",
				 (unsigned int) videostd_set);
			abort ();
		}

		id = DATA_UNIT_ZVBI3_MONOCHROME_SAMPLES_525;
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
		fprintf (stderr,  __FUNCTION__,
			 "%s: Line number %u exceeds limits "
			 "%u ... %u, %u ... %u",
			 line, 0, 31, f2start, f2start + 31);
		abort ();
	}

	s = *samples;
	s_left = *samples_left;

	if (offset + samples_size > 720) {
		fprintf (stderr, __FUNCTION__,
			 "%s: offset %u + samples_size %u > 720\n",
			 offset, samples_size);
		abort ();
	}

	if (s_left > samples_size) {
		fprintf (stderr, __FUNCTION__,
			 "%s: samples_left %u > samples_size %u",
			 s_left, samples_size);
		abort ();
	}

	if (data_identifier >= DATA_ID_EBU_TELETEXT_BEGIN
	    && data_identifier < DATA_ID_EBU_TELETEXT_END)
		min_space = 7;
	else
		min_space = 2 + 0x2C;

	offset += samples_size - s_left;

	while (s_left > 0) {
		unsigned int offset;
		unsigned int n_pixels;

		if (min_space > p_left) {
			/* EN 301 775 section 4.3.1: Data units
			   cannot cross PES packet boundaries. */
			/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
			   stuffing byte: 0xFF */
			memset (p, 0xFF, p_left);

			p += p_left;
			p_left = 0;

			goto finish;
		}

		if (min_space > 7) {
			uint8_t *end;

			n_pixels = MIN (s_left, 2 + 0x2C - 6);
			n_pixels = MIN (n_pixels, p_left - 6);

			/* data_unit_id [8], data_unit_length [8],
			   first_segment_flag [1], last_segment_flag [1],
			   field_parity [1], line_offset [5],
			   first_pixel_position [16], n_pixels [8] */
			p[0] = id;
			p[1] = 0x2C;
			p[2] = line
				+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6);
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
			p[2] = line
				+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6);
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

/*
	Demultiplexer
*/

struct demux {

};






















struct vbi3_dvb_demux {
	/* Wrap-around buffer. Must hold one PES packet or header. */
	uint8_t			wrap_buffer[65536 + 256];

	/* Sliced data buffer. DVB should transmit less than 2 * 32 lines
	   per frame. We fail on pathological cases. */
	vbi3_sliced		sliced[64];

	/* Raw VBI buffer. DVB should not transmit more than one or
	   two lines due to bandwith constraints, but we prepare for
	   a raw VBI file format. */
	uint8_t			raw[64][720];

	uint8_t *		wp;

	unsigned int		skip;
	unsigned int		consume;
	unsigned int		lookahead;

	unsigned int		leftover;

	vbi3_sliced *		sp;
	uint8_t *		rp;

	unsigned int		last_line;

	unsigned int		raw_offset;
};









vbi3_bool
vbi3_dvb_demultiplex_sliced	(const vbi3_sliced **	sliced,
				 unsigned int *		sliced_left,
				 uint8_t **		packet,
				 unsigned int *		packet_left,
				 vbi3_service_set	service_set)
{
}










struct vbi3_dvb_demux {
	/* Wrap-around buffer. Must hold at most one PES packet
	   plus header. First field for proper alignment. */
	uint8_t			wrap_buffer[65536 + 256];

	/* Sliced data buffer. DVB should transmit less than 2 * 32 lines
	   per frame. We fail on pathological cases. */
	vbi3_sliced		sliced[64];

	/* Raw VBI buffer. DVB should not transmit more than one or
	   two lines due to bandwith constraints, but more is useful
	   when we use DVB VBI as raw VBI file format. */
	uint8_t			raw[64][720];

	/* Wrap-around. */

	uint8_t *		wp;

	unsigned int		skip;
	unsigned int		consume;
	unsigned int		lookahead;

	unsigned int		leftover;

	/* Data units. */

	vbi3_sliced *		sp;
	uint8_t *		rp;

	unsigned int		last_line;

	unsigned int		raw_offset;
};

static vbi3_sliced *
sliced_address			(vbi3_dvb_demux *	dx,
				 unsigned int		lofp,
				 unsigned int		system)
{
	static const unsigned int start [2][2] = {
		{ 0, 263 },
		{ 0, 313 },
	};
	unsigned int line_offset;
	vbi3_sliced *s;

	if (dx->sp >= dx->sliced + N_ELEMENTS (dx->sliced))
		return NULL;

	line_offset = lofp & 31;

	if (line_offset > 0) {
		unsigned int field_parity;
		unsigned int line;

		field_parity = !(lofp & (1 << 5));

		line = line_offset + start[system][field_parity];

		if (line > dx->last_line) {
			dx->last_line = line;

			s = dx->sp++;
			s->line = line;
		} else {
			/* EN 301 775 section 4.1: ascending line
			   order, no line twice. But we're generous. */

			for (s = dx->sliced; s < dx->sp; ++s)
				if (line <= s->line)
					break;

			if (line < s->line) {
				memmove (s, s + 1, (dx->sp - s) * sizeof (*s));
				++dx->sp;
				s->line = line;
			}
		}
	} else {
		/* Unknown line. */

		s = dx->sp++;
		s->line = 0;
	}

	return s;
}

static vbi3_bool
samples				(vbi3_dvb_demux *	dx,
				 uint8_t *		p,
				 unsigned int		system)
{
	vbi3_sliced *s;
	unsigned int offset;
	unsigned int n_pixels;
	unsigned int line;

	if (!(s = sliced_address (dx, p[2], system)))
		return FALSE;

	offset = p[3] * 256 + p[4];
	n_pixels = p[5];

	if ((n_pixels - 1) > 250
	    || (offset + n_pixels) > 720) {
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
		} else if (offset != dx->raw_offset) {
			/* Continuity lost, discard incomplete line. */

			memset (dx->rp, 0, 720);
			dx->raw_offset = 0;

			return TRUE;
		}
	}

	s->id = VBI3_SLICED_UNKNOWN;

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

static vbi3_bool
data_units			(vbi3_dvb_demux *	dx,
				 uint8_t *		p,
				 unsigned int		left)
{
	while (left > 0) {
		unsigned int data_unit_id;
		unsigned int data_unit_length;
		vbi3_sliced *s;
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
			/* Should be == but we're generous. */
			if (data_unit_length < 1 + 1 + 42)
				return FALSE;

			/* We cannot transcode custom framing codes,
			   skip this data unit. */
			if (0xE4 != p[3]) /* vbi3_rev8 (0x27) */
				break;

			if (!(s = sliced_address (dx, p[2], 1)))
				return FALSE;

			s->id = VBI3_SLICED_TELETEXT_B_625;

			for (i = 0; i < 42; ++i)
				s->data[i] = vbi3_rev8 (p[4 + i]);

			break;

		case DATA_UNIT_VPS:
			if (data_unit_length < 1 + 13)
				return FALSE;

			if (!(s = sliced_address (dx, p[2], 1)))
				return FALSE;

			s->id = (s->line >= 313) ?
				VBI3_SLICED_VPS_F2 : VBI3_SLICED_VPS;

			for (i = 0; i < 13; ++i)
				s->data[i] = vbi3_rev8 (p[3 + i]);

			break;

		case DATA_UNIT_WSS:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced_address (dx, p[2], 1)))
				return FALSE;

			s->id = VBI3_SLICED_WSS_625;

			s->data[0] = vbi3_rev8 (p[3]);
			s->data[1] = vbi3_rev8 (p[4]);

			break;

		case DATA_UNIT_ZVBI3_WSS_CPR1204:
			if (data_unit_length < 1 + 3)
				return FALSE;

			if (!(s = sliced_address (dx, p[2], 0)))
				return FALSE;

			s->id = VBI3_SLICED_WSS_CPR1204;

			s->data[0] = vbi3_rev8 (p[3]);
			s->data[1] = vbi3_rev8 (p[4]);
			s->data[2] = vbi3_rev8 (p[5]);

			break;

		case DATA_UNIT_ZVBI3_CAPTION_625:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced_address (dx, p[2], 0)))
				return FALSE;

			s->id = VBI3_SLICED_CAPTION_525;

			s->data[0] = vbi3_rev8 (p[3]);
			s->data[1] = vbi3_rev8 (p[4]);

			break;

		case DATA_UNIT_CLOSED_CAPTIONING:
			if (data_unit_length < 1 + 2)
				return FALSE;

			if (!(s = sliced_address (dx, p[2], 1)))
				return FALSE;

			s->id = VBI3_SLICED_CAPTION_625;

			s->data[0] = vbi3_rev8 (p[3]);
			s->data[1] = vbi3_rev8 (p[4]);

			break;

		case DATA_UNIT_MONOCHROME_SAMPLES:
			if (data_unit_length < p[5] + 4)
				return FALSE;

			if (!store_samples (dx, p, 1))
				return FALSE;

			break;

		case DATA_UNIT_ZVBI3_MONOCHROME_SAMPLES_525:
			if (data_unit_length < p[5] + 4)
				return FALSE;

			if (!store_samples (dx, p, 0))
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

static vbi3_bool
timestamp			(int64_t *		pts,
				 unsigned int		mark,
				 const uint8_t *	p)
{
	unsigned int t;

	t = p[0];

	if (mark != (t & 0xF1))
		return FALSE;

	*pts = ((int64_t) t & 0x0E) << (30 - 1);

	t  =  p[1] << 23;
	t |= (p[2] & ~1) << (15 - 1);
	t |=  p[3] << 8;
	t |= (p[4] & ~1) >> 1;

	*pts |= t;

	return TRUE;
}

vbi3_bool
demux				(vbi3_dvb_demux *	dx,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left)
{
	uint8_t *s;
	uint8_t *s_left;
	uint8_t *p;
	uint8_t *p_end;

	s = *buffer;
	s_left = *buffer_left;

	for (;;) {
		unsigned int available;
		unsigned int required;
		unsigned int packet_length;

		/* Wrap-around */

		if (dx->skip > 0) {
			if (dx->skip > dx->leftover) {
				dx->skip -= dx->leftover;
				dx->leftover = 0;

				if (dx->skip > s_left) {
					dx->skip -= s_left;

					*buffer += s_left;
					*buffer_left = 0;

					break;
				}

				s += dx->skip;
				s_left -= dx->skip;
			} else {
				dx->leftover -= dx->skip;
			}

			dx->skip = 0;
		}

		available = dx->leftover + s_left;
		required = dx->consume + dx->lookahead;

		if (required > available
		    || available > *buffer_left) {
			/* Not enough data at p, or we have bytes left
			   over from the previous buffer, must wrap. */

			if (required > dx->leftover) {
				/* Need more data in the wrap_buffer. */

				memmove (dx->wrap_buffer,
					 dx->wp - dx->leftover,
					 dx->leftover);

				dx->wp = dx->wrap_buffer + dx->leftover;

				required -= dx->leftover;

				if (required > s_left) {
					memcpy (dx->wp, p, s_left);
					dx->wp += s_left;

					dx->leftover += s_left;

					*buffer += s_left;
					*buffer_left = 0;

					break;
				}

				memcpy (dx->wp,	p, required);
				dx->wp += required;

				dx->leftover = dx->lookahead;

				s += required;
				s_left -= required;

				p = dx->wrap_buffer;
				p_end = dx->wp - dx->lookahead + 1;
			} else {
				p = dx->wp - dx->leftover;
				p_end = dx->wp - dx->lookahead + 1;

				dx->leftover -= consume;
			}
		} else {
			/* All the required bytes are in this buffer and
			   we have a complete copy of the wrap_buffer
			   leftover bytes before p. */

			p = s - dx->leftover;

			/* End of the range we can scan with lookahead. */
			p_end = s + s_left - dx->lookahead + 1;

			if (dx->consume > dx->leftover) {
				unsigned int advance;

				advance = dx->consume - dx->leftover;

				s += advance;
				s_left -= advance;

				dx->leftover = 0;
			} else {
				dx->leftover -= consume;
			}
		}

		if (dx->consume > 0) {
			vbi3_bool success;

			success = data_fields (dx, p, dx->consume);

			p += dx->consume; /* lookahead header */
			dx->consume = 0;

			if (success) {
				if (callback) {
					callback (&dx->sliced,
						  dx->sp - dx->sliced,
						  NULL);
				} else {
					*sliced = &dx->sliced;
					*sliced_lines = dx->sp - dx->sliced;
					*raw = NULL;

					return TRUE;
				}
			}
		}

		/* Header scan */

		dx->skip = p_end - p;

		for (;;) {
			/* packet_start_code_prefix [24] == 0x000001,
			   stream_id [8] == PRIVATE_STREAM_1 */

			if (likely (p[2] & ~1)) {
				/* Not 000001 or xx0000 or xxxx00. */
				p += 3;
			} else if (0 != (p[0] | p[1]) || 1 != p[2]) {
				++p;
			} else if (PRIVATE_STREAM_1 == p[3]) {
				break;
			} else if (p[3] >= 0xBC) {
				unsigned int skip;

				skip = p[4] * 256 + p[5] /* packet_length */
					+ 6 /* header */;

				if (p_end - p > skip) {
					p += skip;
				} else {
					dx->skip = skip;
					goto outer_continue;
				}
			}

			if (unlikely (p >= p_end)) {
				goto outer_continue;
			}
		}

		/* Packet header */

		packet_length = p[4] * 256 + p[5];

		/* '10', PES_scrambling_control [2] == 0,
		   PES_priority, data_alignment_indicator == 1,
		   copyright, original_or_copy */
		if (0x84 != (p[6] & 0xF4)) {
			dx->skip = packet_length + 6;
			continue;
		}

		switch (p[7] >> 6) {
		case 2:	/* PTS 0010 xxx 1 ... */
			if (!timestamp (&dx->pts, 0x21, p + 9)) {
				dx->skip = packet_length + 5;
				continue;
			}

			break;

		case 3:	/* PTS 0011 xxx 1 ... DTS ... */
			if (!timestamp (&dx->pts, 0x31, p + 9)) {
				dx->skip = packet_length + 5;
				continue;
			}

			break;

		default:
			dx->pts = 0;
			break;
		}

		dx->skip = header_length;
		dx->consume = packet_length;

 outer_continue:
	}

	return FALSE;
}

vbi3_bool
vbi3_dvb_demux_cor		(vbi3_dvb_demux *	dx,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_lines,
				 const uint8_t **	raw,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left)
{
	assert (NULL != dx);
	assert (NULL != buffer);
	assert (NULL != buffer_left);

	if (demux (dx, &buffer, &buffer_size)) {
		if (sliced && sliced_lines) {
			*sliced = &dx->sliced;
			*sliced_lines = dx->sp - dx->sliced;
		}

		if (raw) {
			*raw = NULL;
		}

		return TRUE;
	}

	return FALSE;
}

vbi3_bool
vbi3_dvb_demux_demux		(vbi3_dvb_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	assert (NULL != dx);
	assert (NULL != buffer);
	assert (NULL != dx->callback);

	demux (dx, &buffer, &buffer_size);

	return TRUE;
}

/** @internal */
void
_vbi3_xds_demux_destroy		(vbi3_xds_demux *	xd)
{
	assert (NULL != xd);

	CLEAR (*xd);
}

/** @internal */
vbi3_bool
_vbi3_xds_demux_init		(vbi3_xds_demux *	xd,
				 vbi3_xds_demux_cb *	callback,
				 void *			user_data)
{
	assert (NULL != xd);
	assert (NULL != callback);

	vbi3_xds_demux_reset (xd);

	xd->callback = callback;
	xd->user_data = user_data;

	return TRUE;
}

/**
 * @param xd XDS demultiplexer context allocated with
 *   vbi3_xds_demux_new(), can be @c NULL.
 *
 * Frees all resources associated with @a xd.
 */
void
vbi3_xds_demux_delete		(vbi3_xds_demux *	xd)
{
	if (NULL == xd)
		return;

	_vbi3_xds_demux_destroy (xd);

	vbi3_free (xd);		
}

/**
 * @param callback Function to be called by vbi3_xds_demux_demux() when
 *   a new packet is available.
 * @param user_data User pointer passed through to @a callback function.
 *
 * Allocates a new Extended Data Service (EIA 608) demultiplexer.
 *
 * @returns
 * Pointer to newly allocated XDS demux context which must be
 * freed with vbi3_xds_demux_delete() when done. @c NULL on failure
 * (out of memory).
 */
vbi3_xds_demux *
vbi3_xds_demux_new		(vbi3_xds_demux_cb *	callback,
				 void *			user_data)
{
	vbi3_xds_demux *xd;

	assert (NULL != callback);

	if (!(xd = vbi3_malloc (sizeof (*xd)))) {
		error ("Out of memory (%u bytes)", sizeof (*xd));
		return NULL;
	}

	_vbi3_xds_demux_init (xd, callback, user_data);

	return xd;
}

#endif
