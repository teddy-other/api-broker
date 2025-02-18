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
 * broker_process_info.h -
 */

#ifndef	_BROKER_PROCESS_INFO_H_
#define	_BROKER_PROCESS_INFO_H_

#ident "$Id$"

#ifdef GET_PSINFO
#if !defined(SOLARIS) && !defined(HPUX)
#error NOT IMPLEMENTED
#endif
#endif

typedef struct t_psinfo T_PSINFO;
struct t_psinfo
{
  int num_thr;
  int cpu_time;
  float pcpu;
};

int get_psinfo (int pid, T_PSINFO * ps_info);

#endif /* _BROKER_PROCESS_INFO_H_ */
