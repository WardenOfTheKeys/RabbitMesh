// Just enough of miniz.h for the host tests: the inflate half (tinfl).
#pragma once
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#include "miniz_common.h"
#include "miniz_tinfl.h"
