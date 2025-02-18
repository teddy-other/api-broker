/*
 * Copyright 2024 CUBRID Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */


/*
 * cas_handle.h -
 */

#ifndef	_CAS_HANDLE_H_
#define	_CAS_HANDLE_H_

#include "s62ext.h"

#ident "$Id$"

#define SRV_HANDLE_QUERY_SEQ_NUM(SRV_HANDLE)    \
        ((SRV_HANDLE) ? (SRV_HANDLE)->query_seq_num : 0)

typedef struct t_prepare_call_info T_PREPARE_CALL_INFO;
struct t_prepare_call_info
{
  void *dbval_ret;
  void *dbval_args;
  char *param_mode;
  int num_args;
  int is_first_out;
};

typedef struct t_col_update_info T_COL_UPDATE_INFO;
struct t_col_update_info
{
  char *attr_name;
  char *class_name;
  char updatable;
};

typedef union db_data DB_DATA;
union db_data
{
  int i;
  short sh;
  int64_t bi;
  float f;
  double d;
  void *p;
};

typedef struct db_value DB_VALUE;
struct db_value
{
  unsigned short db_type;
//  my_bool is_null;
  void *buf;			/* data pointer */
  DB_DATA data;
  unsigned long size;
  bool need_clear;
};

typedef struct t_query_result T_QUERY_RESULT;
struct t_query_result
{
  void *result;
  char *null_type_column;
  T_COL_UPDATE_INFO *col_update_info;
  void *column_info;
  int copied;
  int tuple_count;
  S62_STATEMENT *stmt_id;
  int num_column;
  char stmt_type;
  char col_updatable;
  char include_oid;
  bool is_holdable;
};

typedef struct t_srv_handle T_SRV_HANDLE;
struct t_srv_handle
{
  int id;
  void *session;		/* query : DB_SESSION* schema : schema info table pointer */
#if defined (FOR_API_CAS)
  S62_STATEMENT *stmt_id;
  int stmt_type;
  T_QUERY_RESULT *q_result;
#else
  T_PREPARE_CALL_INFO *prepare_call_info;
  T_QUERY_RESULT *q_result;
#endif
  void *cur_result;		/* query : &(q_result[cur_result]) schema info : &(session[cursor_pos]) */
  char *sql_stmt;
  void **classes;
  int *classes_chn;
  int cur_result_index;
  int num_q_result;
  bool has_result_set;
  int num_markers;
  int max_col_size;
  int cursor_pos;
  int schema_type;
  int sch_tuple_num;
  int max_row;
  int num_classes;
  unsigned int query_seq_num;
  char prepare_flag;
  char is_prepared;
  char is_updatable;
  char query_info_flag;
  char is_pooled;
  char need_force_commit;
  char auto_commit_mode;
  char forward_only_cursor;
  bool use_plan_cache;
  bool use_query_cache;
  bool is_fetch_completed;
  bool is_holdable;
  bool is_from_current_transaction;
};

extern int hm_new_srv_handle (T_SRV_HANDLE ** new_handle, unsigned int seq_num);
extern void hm_srv_handle_free (int h_id);
extern void hm_srv_handle_free_all (bool free_holdable);
extern void hm_srv_handle_qresult_end_all (bool end_holdable);
extern T_SRV_HANDLE *hm_find_srv_handle (int h_id);
extern void hm_qresult_clear (T_QUERY_RESULT * q_result);
extern void hm_qresult_end (T_SRV_HANDLE * srv_handle, char free_flag);
extern void hm_session_free (T_SRV_HANDLE * srv_handle);
extern void hm_col_update_info_clear (T_COL_UPDATE_INFO * col_update_info);
#if defined (ENABLE_UNUSED_FUNCTION)
extern void hm_srv_handle_set_pooled (void);
#endif

extern void hm_set_current_srv_handle (int h_id);

extern int hm_srv_handle_get_current_count (void);
extern void hm_srv_handle_unset_prepare_flag_all (void);
#endif /* _CAS_HANDLE_H_ */
