/*
 *  Real Time Encoding Library
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: rte.c,v 1.18 2002-09-26 20:47:35 mschimek Exp $ */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "rtepriv.h"

#define xc context->_class
#define dc codec->_class

/**
 * @mainpage RTE - Real Time Audio/Video Encoding Library
 *
 * @author Iñaki García Etxebarria<br>Michael H. Schimek<br>
 *   FFMpeg backend by Gérard Lantau et al
 *
 * @section intro Introduction
 *
 * The RTE library is a frontend or wrapper of other libraries or
 * programs for real time
 * video and audio compression on Linux. It is designed to interface between
 * codecs and the <a href="http://zapping.sourceforge.net">Zapping TV viewer</a>.
 *
 * RTE has a rather simple design unlike a filter graph.
 * Each recording context has exactly one output and
 * one or more inputs, sending the raw data through codecs for compression
 * and eventually a multiplexer to combine multiple video and audio tracks.
 *
 * Recording can be started and stopped, a pause and preview function are
 * planned for a later date.
 *
 * - @ref client_overview
 * - @ref backend_overview
 */

/**
 * @page client_overview RTE Client Interface Overview
 *
 * This is an overview of the interface between applications and
 * the RTE library. See section <a href="modules.html">Modules</a> for details.
 *
 * RTE client applications <tt>\#include &lt;librte.h&gt;</tt>.
 *
 * A rte_context must be allocated to create an output stream, this
 * can be an elementary stream such as mp3 audio, or a complex
 * multiplexed stream with several video and audio tracks.
 *
 * A rte_codec must be allocated and assigned to a rte_context track to
 * compress raw input to an elementary video or audio stream for this
 * track.
 *
 * @subsection opt Options
 *
 * Both contexts and codecs can have options which are assigned by name
 * like variables. These are often canonical (like @c "sampling_freq",
 * an integer expressing the audio sampling frequency in Hertz),
 * but the same name does not necessarily imply the same semantics
 * in different contexts.
 *
 * @subsection par Input Stream Parameters
 *
 * Codecs need information about the raw data input, for example
 * the image size or audio sample format. These parameters are
 * negotiated with the codec, the client can propose a set of parameters
 * and the codec will return modified parameters shaped by any limitations
 * it has. For example image alignment requirements or supported
 * sample format conversions.
 *
 * The client can also begin negotiation with a blank set of parameters,
 * the returned defaults will be suitable for the requested kind of
 * stream and the given options. Note some input stream parameters and
 * options seemingly specify the same property. In case of @c sampling_freq
 * for example that means the codec resamples from the input sampling
 * frequency (device specific parameter) to the encoded sampling frequency
 * (user option).
 *
 * @subsection io Input/Output Interface
 *
 * An input method must be selected for each rte_codec. Data is
 * always passed in blocks of one video image or a fixed number of
 * audio samples. Either the client or the codec can maintain
 * the buffer memory. RTE does not include driver interfaces,
 * the client is responsible for device or file read access.
 *
 * An output method must be selected for each rte_context.
 * RTE can pass encoded data in blocks or directly write to a file.
 *
 * @subsection start Starting and Stopping
 *
 * This version of RTE will always launch one, sometimes more,
 * subthreads for encoding. This happens when the rte_start()
 * function is called, which returns immediately. The rte_stop()
 * function stops encoding and returns after joining the
 * subthread.
 *
 * Options, parameters and i/o methods cannot be changed
 * when encoding is in progress.
 *
 * @subsection stat Status Report
 *
 * During encoding the context and its codecs can
 * be polled for status information, such as the number of
 * bytes processed or coding time elapsed.
 *
 * @subsection rec_seq Typical Recording Session
 *
 * -# Enumeration of available contexts (i. e. file formats)
 * -# <b>Allocation of a context with rte_context_new()</b>
 * -# Enumeration of context options
 * -# Assignment of context options
 * -# Enumeration of available codecs.<br>
 *   For each elementary stream to be encoded:
 *	-# <b>Selection of the codec to encode a track with rte_set_codec()</b>,
 *         options are reset to their default value
 *	-# Enumeration of codec options
 *	-# Assignment of codec options
 *	-# <b>Input stream parameters negotiation</b>,
 *	   locks the codec options
 *	-# <b>Selection of the input method</b>, locks parameters
 * -# <b>Selection of the output method</b>, locks the context
 *    options and codecs table
 * -# <b>Start of recording with rte_start()</b>, no properties can be changed
 *    until stopping
 * -# Status polling
 * -# <b>Stop recording with rte_stop()</b>
 * -# rte_context_delete()
 *
 * The operations in <b>bold face</b> are mandatory, the rest optional.
 *
 * When a property is locked, changing it resets this and all following
 * properties in the sequence above. Said properties must be
 * renegotiated to proceed. Stopping resets all parameters, input
 * and output methods. Enumeration and get functions are always
 * available.
 */

/**
 * @page backend_overview RTE Backend Interface Overview
 *
 * RTE consists of four layers:
 * - Frontend
 * - Backend
 * - Context
 * - Codec
 *
 * The frontend code implements the interface to RTE clients, hiding
 * all implementation details. It does some housekeeping, sanity checking
 * on client input, and provides helper functions for backends.
 *
 * The frontend maintains multiple backends, possibly through use of
 * plugins, which implement most of the actual RTE functionality on top of
 * the respective compression library or program. That includes for
 * example missing UI functions or backend specific implementation of
 * the RTE i/o functions.
 *
 * Context and codec definitions are cleanly separated, so the code can be
 * kept in isolated modules or merged into context or backend modules.
 *
 * The rte_context and rte_codec structure are inaccessible to the
 * client and shall be used by the backend as documented. The
 * rte_backend_class, rte_context_class and rte_codec_class define
 * the interface of the respective layer to higher layers.
 *
 * @subsection requirements Minimum Requirements
 *
 * - Each backend must support at least one class of context (i. e.
 *   file format) and one codec.
 * - The backend must be able to allocate multiple instances of
 *   contexts and codecs, although no more than one context needs
 *   to encode at a time.
 * - Context and codec options are optional.
 * - Each codec must implement input stream parameter negotiation,
 *   even if the parameters are constant.
 * - Each codec must implement at least the callback-master
 *   input method.
 * - Status report is optional.
 * - All functions which can fail should leave an error description
 *   with rte_error_printf() or other helper functions, in a language
 *   suitable for a user interface.
 * - Labels, tooltips, menu items and error messages should be
 *   internationalized with GNU gettext. Use the _() etc.
 *   macros defined in rtepriv.h.
 *
 * @subsection compilation Backend Installation
 *
 * Put each backend into a separate directory under rte/. The directory
 * name goes into rte/Makefile.am. A backend switch, the Makefile.am's
 * to be built and environment checks go into rte/configure.in.
 * Take ffmpeg as example. In rte/src, add any backend object
 * libs to Makefile.am, extend the backends[] array in %context.c.
 *
 * Backend files <tt>\#include "config.h"</tt> to get configure
 * definitions and <tt>\#include "rtepriv.h"</tt> to get all RTE
 * definitions.
 *
 * For test code see the rte/test directory. Support is available
 * at <a href="mailto:zapping-misc@lists.sourceforge.net">zapping-misc@lists.sourceforge.net</a>.
 */

/** @addtogroup Context Context */
/** @addtogroup Codec Codec */
/** @addtogroup Option Context and Codec Options */
/** @addtogroup Param Raw Input Parameters */
/** @addtogroup IO Input/Output Interfaces */
/** @addtogroup Start Starting and Stopping */
/** @addtogroup Status Context and Codec Status */
/** @addtogroup Error Errors */

const char _rte_intl_domainname[] = PACKAGE;

/**
 * @addtogroup Backend Backend Interface
 *
 * These, together with the public RTE structures, are the definitions
 * of the RTE backend interface from rtepriv.h. Only backends use this,
 * not RTE clients.
 */

/* no public prototype */
void rte_status_query(rte_context *context, rte_codec *codec,
		      rte_status *status, unsigned int size);

/**
 * @internal
 *
 * @param context Initialized rte_context.
 * @param codec Pointer to rte_codec if fetching codec status, else
 *   context status.
 * @param status Buffer to store status structure. 
 * @param size Size of buffer. 
 *
 * This functions returns context or codec status. It is not
 * client visible but wrapped in inline rte_context_status() and
 * rte_codec_status(). The @a size argument is a compile time
 * constant on the client side, this permits upwards compatible
 * extensions to rte_status.
 *
 * Implementation:
 * No keywords because the app must look up values anyway, we're
 * in critical path and a struct is much faster than enum & strcmp.
 * A copy of rte_status is returned because the values are calculated
 * on the fly and must be r/w locked.
 */
void
rte_status_query(rte_context *context, rte_codec *codec,
		 rte_status *status, unsigned int size)
{
	assert(status != NULL);
	assert(size >= sizeof(status->valid));

	if (codec)
		context = codec->context;
	if (!context || !xc->status) {
		status->valid = 0;
		return;
	}

	if (context->state != RTE_STATE_RUNNING) {
		status->valid = 0;
		return;
	}

	if (size > sizeof(*status))
		size = sizeof(*status);

	xc->status(context, codec, status, size);
}

/**
 * @internal
 *
 * @param context Initialized rte_context.
 * @param codec Pointer to rte_codec if these are codec options,
 *   @c NULL if context options.
 * @param optstr Option string.
 *
 * RTE internal function to parse an option string and set
 * the options accordingly.
 *
 * @return
 * @c FALSE if the string is invalid or setting some option failed.
 */
rte_bool
rte_option_string(rte_context *context, rte_codec *codec, const char *optstr)
{
	rte_option_info *oi;
	char *s, *s1, *keyword, *string, quote;
	rte_bool r = TRUE;

	assert(context != NULL);
	assert(optstr != NULL);

	s = s1 = strdup(optstr);

	if (!s) {
		rte_error_printf(context, _("Out of memory."));
		return FALSE;
	}

	do {
		while (isspace(*s))
			s++;

		if (*s == ',' || *s == ';') {
			s++;
			continue;
		}

		if (!*s)
			break;

		keyword = s;

		while (isalnum(*s) || *s == '_')
			s++;

		if (!*s)
			goto invalid;

		*s++ = 0;

		while (isspace(*s) || *s == '=')
			s++;

		if (!*s) {
 invalid:
			rte_error_printf(context, "Invalid option string \"%s\".",
					 optstr);
			break;
		}

		if (codec)
			oi = rte_codec_option_info_by_keyword(codec, keyword);
		else
			oi = rte_context_option_info_by_keyword(context, keyword);

		if (!oi)
			break;

		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
		case RTE_OPTION_MENU:
			if (codec)
				r = rte_codec_option_set(codec,
					keyword, (int) strtol(s, &s, 0));
			else
				r = rte_context_option_set(context,
					keyword, (int) strtol(s, &s, 0));
			break;

		case RTE_OPTION_REAL:
			if (codec)
				r = rte_codec_option_set(codec,
					keyword, (double) strtod(s, &s));
			else
				r = rte_context_option_set(context,
					keyword, (double) strtod(s, &s));
			break;

		case RTE_OPTION_STRING:
			quote = 0;
			if (*s == '\'' || *s == '"')
				quote = *s++;
			string = s;

			while (*s && *s != quote
			       && (quote || (*s != ',' && *s != ';')))
				s++;
			if (*s)
				*s++ = 0;

			if (codec)
				r = rte_codec_option_set(codec, keyword, string);
			else
				r = rte_context_option_set(context, keyword, string);
			break;

		default:
			fprintf(stderr,	"rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}

	} while (r);

	free(s1);

	return r;
}

/*
 *  Error functions
 */

/**
 * @param errstr Place to store the allocated string or @c NULL.
 * @param templ See printf().
 * @param Varargs See printf(). 
 * 
 * RTE internal helper function for backends.
 *
 * Identical to GNU or BSD libc asprintf().
 */
void
rte_asprintf(char **errstr, const char *templ, ...)
{
	char buf[512];
	va_list ap;
	int temp;

	if (!errstr)
		return;

	temp = errno;

	va_start(ap, templ);

	vsnprintf(buf, sizeof(buf) - 1, templ, ap);

	va_end(ap);

	*errstr = strdup(buf);

	errno = temp;
}

static char *
whois(rte_context *context, rte_codec *codec)
{
	char name[80];

	if (codec) {
		rte_codec_info *ci = &codec->_class->_public;

		snprintf(name, sizeof(name) - 1,
			 "codec %s", ci->label ? _(ci->label) : ci->keyword);
	} else if (context) {
		rte_context_info *ci = &context->_class->_public;

		snprintf(name, sizeof(name) - 1,
			 "context %s", ci->label ? _(ci->label) : ci->keyword);
	} else {
		fprintf(stderr, "rte bug: unknown context or codec called error function\n");
		return NULL;
	}

	return strdup(name);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param codec Pointer to rte_codec if this refers to a codec option,
 *  @c NULL if context option.
 * @param keyword Keyword of the option, can be @c NULL or "".
 * 
 * RTE internal helper function for backends.
 *
 * Sets the @a context error string.
 */
void
rte_unknown_option(rte_context *context, rte_codec *codec, const char *keyword)
{
	char *name = whois(context, codec);

	if (!name)
		return;

	if (!keyword)
		rte_error_printf(context, "No option keyword for %s.", name);
	else
		rte_error_printf(context, "'%s' is no option of %s.", keyword, name);

	free(name);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param codec Pointer to rte_codec if this refers to a codec option,
 *  @c NULL if context option.
 * @param keyword Keyword of the option, can be @c NULL or "".
 * @param ... If the option is known, the invalid data (int, double, char *).
 * 
 * RTE internal helper function for backends.
 *
 * Sets the @a context error string.
 */
void
rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...)
{
	char buf[256], *name = whois(context, codec);
	rte_option_info *oi;

	if (!keyword || !keyword[0])
		return rte_unknown_option(context, codec, keyword);

	if (!name)
		return;

	if (codec)
		oi = rte_codec_option_info_by_keyword(codec, keyword);
	else
		oi = rte_context_option_info_by_keyword(context, keyword);

	if (oi) {
		va_list args;
		char *s;

		va_start(args, keyword);

		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
		case RTE_OPTION_MENU:
			snprintf(buf, sizeof(buf) - 1, "'%d'", va_arg(args, int));
			break;
		case RTE_OPTION_REAL:
			snprintf(buf, sizeof(buf) - 1, "'%f'", va_arg(args, double));
			break;
		case RTE_OPTION_STRING:
			s = va_arg(args, char *);
			if (s == NULL)
				strncpy(buf, "NULL", 4);
			else
				snprintf(buf, sizeof(buf) - 1, "'%s'", s);
			break;
		default:
			fprintf(stderr,	"rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			strncpy(buf, "?", 1);
			break;
		}

		va_end(args);
	} else
		strncpy(buf, "??", 2);

	rte_error_printf(context, "Invalid argument %s for option %s of %s.",
			 buf, keyword, name);
	free(name);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param d If non-zero, store pointer to allocated string here. When *d
 *   is non-zero, free(*d) the old string first.
 * @param s String to be duplicated.
 * 
 * RTE internal helper function for backends.
 *
 * Same as the libc strdup(), except for @a d argument and setting
 * the @a context error string on failure.
 * 
 * @return 
 * @c NULL on failure, pointer to malloc()ed string otherwise.
 */
char *
rte_strdup(rte_context *context, char **d, const char *s)
{
	char *new = strdup(s ? s : "");

	if (!new) {
		rte_error_printf(context, _("Out of memory."));
		errno = ENOMEM;
		return NULL;
	}

	if (d) {
		if (*d)
			free(*d);
		*d = new;
	}

	return new;
}

/**
 * @param vec Vector of int values.
 * @param len Length of the vector.
 * @param val Value to be compared.
 * 
 * RTE internal helper function for backends.
 *
 * Find in a vector of int values the entry closest to
 * @a val and return its index, 0 ... n.
 * 
 * @return 
 * Index. Never fails.
 */
unsigned int
rte_closest_int(const int *vec, unsigned int len, int val)
{
	unsigned int i, imin = 0;
	int dmin = INT_MAX;

	assert(vec != NULL && len > 0);

	for (i = 0; i < len; i++) {
		int d = fabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}

/**
 * @param vec Vector of double values.
 * @param len Length of the vector.
 * @param val Value to be compared.
 * 
 * RTE internal helper function for backends.
 *
 * Find in a vector of double values the entry closest to
 * @a val and return its index, 0 ... n.
 * 
 * @return 
 * Index. Never fails.
 */
unsigned int
rte_closest_double(const double *vec, unsigned int len, double val)
{
	unsigned int i, imin = 0;
	double dmin = DBL_MAX;

	assert(vec != NULL && len > 0);

	for (i = 0; i < len; i++) {
		double d = fabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}
