/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2012, ruki All rights reserved.
 *
 * @author		ruki
 * @file		file.c
 * @ingroup 	platform
 */

/* ///////////////////////////////////////////////////////////////////////
 * trace
 */
#undef TB_TRACE_IMPL_TAG
#define TB_TRACE_IMPL_TAG 				"file"

/* ///////////////////////////////////////////////////////////////////////
 * includes
 */
#include "../prefix.h"

/* ///////////////////////////////////////////////////////////////////////
 * types
 */

// the file proactor type
typedef struct __tb_aicp_proactor_file_t
{
	// the proactor base
	tb_aicp_proactor_t 			base;

	// the unix proactor
	tb_aicp_proactor_t* 		uptr;

	// the proactor indx
	tb_size_t 					indx;

	// the aice resp
	tb_queue_t* 				resp;
	
	// the resp mutx
	tb_handle_t 				mutx;

}tb_aicp_proactor_file_t;

/* ///////////////////////////////////////////////////////////////////////
 * implementation
 */
static tb_bool_t tb_aicp_proactor_file_post(tb_aicp_proactor_t* proactor, tb_aice_t const* list, tb_size_t size)
{
	// check
	tb_aicp_proactor_file_t* ptor = (tb_aicp_proactor_file_t*)proactor;
	tb_assert_and_check_return_val(ptor && list && size, tb_false);
	
	// enter 
	if (ptor->mutx) tb_mutex_enter(ptor->mutx);

	// walk list
	tb_size_t i = 0;
	tb_bool_t ok = tb_true;
	for (i = 0; i < size && ok; i++)
	{
		// the aice
		tb_aice_t const* aice = &list[i];

		// post aice
		if (!tb_queue_full(ptor->resp)) 
		{
			// put
			tb_queue_put(ptor->resp, aice);

			// trace
			tb_trace_impl("resp: code: %lu, size: %lu", aice->code, tb_queue_size(ptor->resp));
		}
		else
		{
			// failed
			ok = tb_false;

			// assert
			tb_assert(0);
		}

	}

	// leave 
	if (ptor->mutx) tb_mutex_leave(ptor->mutx);

	// only post wait
	if (ok) tb_aicp_proactor_unix_resp(ptor->uptr, tb_null);

	// ok?
	return ok;
}
static tb_long_t tb_aicp_proactor_file_spak(tb_aicp_proactor_t* proactor, tb_aice_t* resp, tb_long_t timeout)
{
	// check
	tb_aicp_proactor_file_t* ptor = (tb_aicp_proactor_file_t*)proactor;
	tb_assert_and_check_return_val(ptor && resp, -1);

	// enter 
	if (ptor->mutx) tb_mutex_enter(ptor->mutx);

	// post aice
	tb_long_t ok = 0;
	if (!tb_queue_null(ptor->resp)) 
	{
		// get resp
		tb_aice_t const* aice = tb_queue_get(ptor->resp);
		if (aice) 
		{
			// save resp
			*resp = *aice;

			// trace
			tb_trace_impl("spak[%u]: code: %lu, size: %lu", (tb_uint16_t)tb_thread_self(), aice->code, tb_queue_size(ptor->resp));

			// pop it
			tb_queue_pop(ptor->resp);

			// ok
			ok = 1;
		}
	}

	// leave 
	if (ptor->mutx) tb_mutex_leave(ptor->mutx);

	// ok?
	if (ok) 
	{
		// spak it
		switch (resp->code)
		{
		case TB_AICE_CODE_READ:
			{
				// seek
				tb_long_t real = -1;
				if (tb_file_seek(resp->handle, resp->u.read.seek))
				{
					// read
					real = tb_file_read(resp->handle, resp->u.read.data, resp->u.read.size);

					// trace
					tb_trace_impl("read[%p]: %ld", resp->handle, real);
				}

				// ok?
				if (real > 0) 
				{
					resp->u.read.real = real;
					resp->state = TB_AICE_STATE_OK;
				}
				// closed
				else if (!real) resp->state = TB_AICE_STATE_CLOSED;
				// failed?
				else resp->state = TB_AICE_STATE_FAILED;
			}
			break;
		case TB_AICE_CODE_WRIT:
			{
				// seek
				tb_long_t real = -1;
				if (tb_file_seek(resp->handle, resp->u.writ.seek))
				{
					// writ
					real = tb_file_writ(resp->handle, resp->u.writ.data, resp->u.writ.size);
			
					// writ
					tb_trace_impl("writ[%p]: %ld", resp->handle, real);
				}
				
				// ok?
				if (real > 0) 
				{
					resp->u.writ.real = real;
					resp->state = TB_AICE_STATE_OK;
				}
				// closed
				else if (!real) resp->state = TB_AICE_STATE_CLOSED;
				// failed?
				else resp->state = TB_AICE_STATE_FAILED;
			}
			break;
		default:
			ok = -1;
			break;
		}
	}

	// ok?
	return ok;
}
static tb_void_t tb_aicp_proactor_file_exit(tb_aicp_proactor_t* proactor)
{
	tb_aicp_proactor_file_t* ptor = (tb_aicp_proactor_file_t*)proactor;
	if (ptor)
	{
		// trace
		tb_trace_impl("exit");

		// exit resp
		if (ptor->mutx) tb_mutex_enter(ptor->mutx);
		if (ptor->resp) tb_queue_exit(ptor->resp);
		ptor->resp = tb_null;
		if (ptor->mutx) tb_mutex_leave(ptor->mutx);

		// exit mutx
		if (ptor->mutx) tb_mutex_exit(ptor->mutx);
		ptor->mutx = tb_null;

		// exit it
		tb_free(ptor);
	}
}

/* ///////////////////////////////////////////////////////////////////////
 * interfaces
 */
static tb_void_t tb_aicp_proactor_file_init(tb_aicp_proactor_unix_t* uptr)
{
	// check
	tb_assert_and_check_return(uptr && uptr->base.aicp && uptr->base.aicp->maxn);

	// need this proactor?
	tb_check_return( 	!uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_READ]
					|| 	!uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_WRIT]
					);

	// make proactor
	tb_aicp_proactor_file_t* ptor = tb_malloc0(sizeof(tb_aicp_proactor_file_t));
	tb_assert_and_check_return(ptor);

	// init base
	ptor->uptr = uptr;
	ptor->base.aicp = uptr->base.aicp;
	ptor->base.exit = tb_aicp_proactor_file_exit;
	ptor->base.post = tb_aicp_proactor_file_post;
	ptor->base.spak = tb_aicp_proactor_file_spak;

	// init mutx
	ptor->mutx = tb_mutex_init(tb_null);
	tb_assert_and_check_goto(ptor->mutx, fail);

	// init resp
	ptor->resp = tb_queue_init((ptor->base.aicp->maxn << 2) + 16, tb_item_func_ifm(sizeof(tb_aice_t), tb_null, tb_null));
	tb_assert_and_check_goto(ptor->resp, fail);

	// add this proactor to the unix proactor list
	uptr->ptor_list[uptr->ptor_size++] = (tb_aicp_proactor_t*)ptor;
	ptor->indx = uptr->ptor_size;

	// attach index to some aice post
	if (!uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_READ]) uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_READ] = ptor->indx;
	if (!uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_WRIT]) uptr->ptor_post[TB_AICO_TYPE_FILE][TB_AICE_CODE_WRIT] = ptor->indx;

	// ok
	return ;

fail:
	if (ptor) tb_aicp_proactor_file_exit(ptor);
}

