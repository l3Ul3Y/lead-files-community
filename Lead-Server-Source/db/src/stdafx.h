#ifndef __INC_METiN_II_DBSERV_STDAFX_H__
#define __INC_METiN_II_DBSERV_STDAFX_H__

#pragma once

#include "../../libthecore/include/stdafx.h"

#ifndef __WIN32__
#include <semaphore.h>
#else
#define isdigit iswdigit
#define isspace iswspace
#endif

#include "common/length.h"
#include "common/tables.h"
#include "common/singleton.h"
#include "common/utils.h"
#include "common/stl.h"
#include "common/service.h"

// TODO: Remove workaround by using mysqlclient build from vcpkg
//       This is just needed to make the compiler shut up for now.
extern "C" {
	FILE* __cdecl __iob_func(void) {
		static FILE* iob[3] = { stdin, stdout, stderr };
		return iob[0];
	}
}

#endif
