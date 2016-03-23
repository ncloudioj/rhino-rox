#include "rr_rhino_rox.h"
#include "rr_logging.h"
#include "sds.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    sds s = sdsnew("hello");

	rr_log(RR_LOG_DEBUG, s);
	sdsfree(s);
    rr_debug("%d Rhino-Rox is climbing the big tree now.", 10);
    rr_log(RR_LOG_DEBUG, "Rhino is %d years old golden snub-nosed monkey.", 12);
    return 0;
}
