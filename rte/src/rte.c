/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
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
#include "rtepriv.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
  FIXME: Better error reporting.
  TODO: i18n support.
*/

#define xc context->class
#define dc codec->class

static rte_backend_info *
backends[] = 
{
	/* tbd */
};
static const int num_backends = sizeof(backends)/sizeof(backends[0]);

/* inits the backends if not already inited. */
static void
rte_init(void)
{
	static rte_bool inited = FALSE;

	if (!inited) {
		int i;
		for (i = 0; i<num_backends; i++)
			if (backends[i]->init)
				backends[i]->init();

		inited = TRUE;
	}
}

rte_context_info *
rte_context_info_enum(int index)
{
	rte_context_info *rxi;
	int i, j;

	rte_init();
	
	for (j = 0; j < num_backends; j++)
		for (i = 0; backends[j]->context_enum != NULL
			     && (rxi = backends[j]->context_enum(i)); i++)
			if (index-- == 0)
				return rxi;
	return NULL;
}

rte_context_info *
rte_context_info_keyword(const char *keyword)
{
	rte_context_info *rxi;
	int i, j;

	rte_init();

	for (j = 0; j < num_backends; j++)
		for (i = 0; backends[j]->context_enum != NULL
			     && (rxi = backends[j]->context_enum(i)); i++)
			if (strcmp(keyword, rxi->keyword) == 0)
				return rxi;
	return NULL;
}

rte_context_info *
rte_context_info_context(rte_context *context)
{
	nullcheck(context, return NULL);

	return &xc->public;
}

rte_context *
rte_context_new(const char *keyword, rte_pointer user_data)
{
	rte_context *context = NULL;
	int j;

	rte_init();

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_new
		    && (context = backends[j]->context_new(keyword))) {
			/* set output and we are ready */
			context->status = RTE_STATUS_PARAM;
			return context;
		}

	return NULL;
}

void
rte_context_delete(rte_context *context)
{
	nullcheck(context, return);

	if (context->status == RTE_STATUS_RUNNING)
		rte_stop(context);
	else if (context->status == RTE_STATUS_PAUSED)
		/* FIXME */;

	if (context->error) {
		free(context->error);
		context->error = NULL;
	}

	xc->delete(context);
}

void
rte_context_set_user_data(rte_context *context, rte_pointer user_data)
{
	context->user_data = user_data;
}

rte_pointer
rte_context_get_user_data	(rte_context *context)
{
	return context->user_data;
}

rte_codec_info *
rte_codec_info_enum(rte_context *context, int index)
{
	nullcheck(context, return NULL);

	if (!xc->codec_enum)
		return NULL;

	return xc->codec_enum(context, index);
}

rte_codec_info *
rte_codec_info_keyword(rte_context *context,
		       const char *keyword)
{
	rte_codec_info *rci;
	int i;

	nullcheck(context, return NULL);

	if (!xc->codec_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(rci = xc->codec_enum(context, i))
		    || strcmp(keyword, rci->keyword) == 0)
			break;
	return rci;
}

rte_codec_info *
rte_codec_info_codec(rte_codec *codec)
{
	nullcheck(codec, return NULL);

	return &dc->public;
}

rte_codec *
rte_codec_set(rte_context *context, int stream_index,
	      const char *codec_keyword)
{
	nullcheck(context, return NULL);
	nullcheck(xc->codec_set, return NULL);

	return xc->codec_set(context, stream_index, codec_keyword);
}

rte_codec *
rte_codec_get(rte_context *context, rte_stream_type stream_type,
	      int stream_index)
{
	nullcheck(context, return NULL);
	nullcheck(xc->codec_get, return NULL);

	return xc->codec_get(context, stream_type, stream_index);
}

void
rte_codec_set_user_data(rte_codec *codec, rte_pointer data)
{
	nullcheck(codec, return);

	codec->user_data = data;
}

rte_pointer
rte_codec_get_user_data(rte_codec *codec)
{
	nullcheck(codec, return NULL);

	return codec->user_data;
}

rte_bool
rte_codec_set_parameters(rte_codec *codec, rte_stream_parameters *rsp)
{
	nullcheck(codec, return FALSE);
	nullcheck(rsp, return FALSE);
	nullcheck(dc->set_parameters, return FALSE);

	return dc->set_parameters(codec, rsp);
}

void
rte_codec_get_parameters(rte_codec *codec, rte_stream_parameters *rsp)
{
	nullcheck(codec, return);
	nullcheck(rsp, return);
	nullcheck(dc->get_parameters, return);

	dc->get_parameters(codec, rsp);

}

void
rte_set_input_callback_buffered(rte_codec *codec,
				rteBufferCallback get_cb,
				rteBufferCallback unref_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_CB;
	codec->input.cb.get = get_cb;
	codec->input.cb.unref = unref_cb;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_callback_data(rte_codec *codec,
			    rteDataCallback data_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_CD;
	codec->input.cd.get = data_cb;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_push_buffered(rte_codec *codec,
			    rteBufferCallback unref_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_PB;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_push_data(rte_codec *codec)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_PD;

	codec->status = RTE_STATUS_READY;
}

void
rte_push_buffer(rte_codec *codec, rte_buffer *buffer,
		rte_bool blocking)
{
	/* FIXME */
}

rte_pointer
rte_push_data(rte_codec *codec, rte_pointer data, double timestamp,
	      rte_bool blocking)
{
	/* FIXME */
	return NULL;
}

void
rte_set_output_callback(rte_context *context,
			rteWriteCallback write_cb,
			rteSeekCallback seek_cb)
{
	nullcheck(context, return);

	context->write = write_cb;
	context->seek = seek_cb;

	context->status = RTE_STATUS_READY;
}

rte_option_info *
rte_codec_option_info_enum(rte_codec *codec, int index)
{
	nullcheck(codec, return 0);

	if (!dc->option_enum)
		return NULL;

	return dc->option_enum(codec, index);
}

rte_option_info *
rte_codec_option_info_keyword(rte_codec *codec, const char *keyword)
{
	rte_option_info *ro;
	int i;

	nullcheck(codec, return 0);

	if (!dc->option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(ro = dc->option_enum(codec, i))
		    || strcmp(keyword, ro->keyword) == 0)
			break;
	return ro;
}

rte_bool
rte_codec_option_get(rte_codec *codec, const char *keyword,
		     rte_option_value *v)
{
	nullcheck(codec, return 0);

	if (!dc->option_get)
		return FALSE;

	return dc->option_get(codec, keyword, v);
}

rte_bool
rte_codec_option_set(rte_codec *codec, const char *keyword, ...)
{
	va_list args;
	rte_bool r;

	nullcheck(codec, return FALSE);

	if (!dc->option_set)
		return FALSE;

	va_start(args, keyword);

	r = dc->option_set(codec, keyword, args);

	va_end(args);

	return FALSE;
}

char *
rte_codec_option_print(rte_codec *codec, const char *keyword, ...)
{
	va_list args;
	char *r;

	nullcheck(codec, return NULL);

	if (!dc->option_print)
		return NULL;

	va_start(args, keyword);

	r = dc->option_print(codec, keyword, args);

	va_end(args);

	return r;
}

rte_option_info *
rte_context_option_info_enum(rte_context *context, int index)
{
	nullcheck(context, return 0);

	if (!xc->option_enum)
		return NULL;

	return xc->option_enum(context, index);
}

rte_option_info *
rte_context_option_info_keyword(rte_context *context, const char *keyword)
{
	rte_option_info *ro;
	int i;

	nullcheck(context, return 0);

	if (!xc->option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(ro = xc->option_enum(context, i))
		    || strcmp(keyword, ro->keyword) == 0)
			break;
	return ro;
}

rte_bool
rte_context_option_get(rte_context *context, const char *keyword,
		     rte_option_value *v)
{
	nullcheck(context, return 0);

	if (!xc->option_get)
		return FALSE;

	return xc->option_get(context, keyword, v);
}

rte_bool
rte_context_option_set(rte_context *context, const char *keyword, ...)
{
	va_list args;
	rte_bool r;

	nullcheck(context, return FALSE);

	if (!xc->option_set)
		return FALSE;

	va_start(args, keyword);

	r = xc->option_set(context, keyword, args);

	va_end(args);

	return FALSE;
}

char *
rte_context_option_print(rte_context *context, const char *keyword, ...)
{
	va_list args;
	char *r;

	nullcheck(context, return NULL);

	if (!xc->option_print)
		return NULL;

	va_start(args, keyword);

	r = xc->option_print(context, keyword, args);

	va_end(args);

	return r;
}

rte_status_info *
rte_context_status_enum(rte_context *context, int n)
{
	nullcheck(context, return NULL);
	nullcheck(xc->status_enum, return NULL);

	return xc->status_enum(context, n);
}

rte_status_info *
rte_context_status_keyword(rte_context *context, const char *keyword)
{
	rte_status_info *si;
	int i;

	nullcheck(context, return NULL);
	nullcheck(xc->status_enum, return NULL);

	for (i = 0;; i++)
	        if (!(si = xc->status_enum(context, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}

rte_status_info *
rte_codec_status_enum(rte_codec *codec, int n)
{
	nullcheck(codec, return NULL);
	nullcheck(dc->status_enum, return NULL);

	return dc->status_enum(codec, n);
}

rte_status_info *
rte_codec_status_keyword(rte_codec *codec, const char *keyword)
{
	rte_status_info *si;
	int i;

	nullcheck(codec, return NULL);
	nullcheck(dc->status_enum, return NULL);

	for (i = 0;; i++)
	        if (!(si = dc->status_enum(codec, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}

void
rte_status_free(rte_status_info *status)
{
	nullcheck(status, return);

	if (status->type == RTE_STRING)
	  free(status->val.str);

	free(status);
}

rte_bool
rte_start(rte_context *context)
{
	rte_bool result;

	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_READY) {
		if (context->status == RTE_STATUS_RUNNING)
			rte_error(context, "Already encoding!");
		else if (context->status == RTE_STATUS_PAUSED)
			rte_error(context, "Paused, use rte_resume");
		else
			rte_error(context,
				  "You must context_set_output first");
		return FALSE;
	}

	/* FIXME: to do */

	result = xc->start(context);

	if (result)
		context->status = RTE_STATUS_RUNNING;

	return result;
}

void
rte_stop(rte_context *context)
{
	nullcheck(context, return);

	if (context->status < RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_READY;
}

void
rte_pause(rte_context *context)
{
	nullcheck(context, return);

	if (context->status != RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_PAUSED;
}

rte_bool
rte_resume(rte_context *context)
{
	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_PAUSED) {
		rte_error(context, "Not paused!!");
		return FALSE;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_RUNNING;

	return TRUE;
}
