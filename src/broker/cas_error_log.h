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
 * cas_error_log.h -
 */

#ifndef	_CAS_ERROR_LOG_H_
#define	_CAS_ERROR_LOG_H_

#ident "$Id$"

extern void cas_error_log_open (char *br_name);
extern void cas_error_log_close (bool flag);
extern void cas_error_log_write (int dbms_errno, const char *dbms_errmsg);
extern char *cas_error_log_get_eid (char *buf, size_t bufsz);

#endif /* _CAS_ERROR_LOG_H_ */
